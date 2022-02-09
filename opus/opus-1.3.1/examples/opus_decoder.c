#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ogg/ogg.h>
#include <limits.h>

#include "opus.h"
//#include "debug.h"
#include "opus_types.h"
//#include "opus_private.h"
#include "opus_multistream.h"

#define SAMPLES_PER_FRAME 1152
#define OPUS_SAMPLES_IN_BUF ((SAMPLES_PER_FRAME) << 1)
#define OPUS_PCM_BUF_SIZE ((OPUS_SAMPLES_IN_BUF) << 2)

/**The maximum number of channels in an Ogg Opus stream.*/
#define OPUS_CHANNEL_COUNT_MAX (255)

# define OP_NCHANNELS_MAX (8)

# define OP_MIN(_a,_b)        ((_a)<(_b)?(_a):(_b))
# define OP_MAX(_a,_b)        ((_a)>(_b)?(_a):(_b))
# define OP_CLAMP(_lo,_x,_hi) (OP_MAX(_lo,OP_MIN(_x,_hi)))

/*Matrices for downmixing from the supported channel counts to stereo.
  The matrices with 5 or more channels are normalized to a total volume of 2.0,
   since most mixes sound too quiet if normalized to 1.0 (as there is generally
   little volume in the side/rear channels).
  Hence we keep the coefficients in Q14, so the downmix values won't overflow a
   32-bit number.*/
static const opus_int16 OP_STEREO_DOWNMIX_Q14
[OP_NCHANNELS_MAX - 2][OP_NCHANNELS_MAX][2] =
{
	/*3.0*/
	{
		{9598, 0}, {6786, 6786}, {0, 9598}
	},
	/*quadrophonic*/
	{
		{6924, 0}, {0, 6924}, {5996, 3464}, {3464, 5996}
	},
	/*5.0*/
	{
		{10666, 0}, {7537, 7537}, {0, 10666}, {9234, 5331}, {5331, 9234}
	},
	/*5.1*/
	{
		{8668, 0}, {6129, 6129}, {0, 8668}, {7507, 4335}, {4335, 7507}, {6129, 6129}
	},
	/*6.1*/
	{
		{7459, 0}, {5275, 5275}, {0, 7459}, {6460, 3731}, {3731, 6460}, {4568, 4568},
		{5275, 5275}
	},
	/*7.1*/
	{
		{6368, 0}, {4502, 4502}, {0, 6368}, {5515, 3183}, {3183, 5515}, {5515, 3183},
		{3183, 5515}, {4502, 4502}
	}
};

struct opus_head
{
	/**The Ogg Opus format version, in the range 0...255.
	 The top 4 bits represent a "major" version, and the bottom four bits
	 represent backwards-compatible "minor" revisions.
	 The current specification describes version 1.
	 This library will recognize versions up through 15 as backwards compatible
	 with the current specification.
	 An earlier draft of the specification described a version 0, but the only
	 difference between version 1 and version 0 is that version 0 did
	 not specify the semantics for handling the version field.*/
	int           version;
	/**The number of channels, in the range 1...255.*/
	int           channel_count;
	/**The number of samples that should be discarded from the beginning of the stream.*/
	unsigned int   pre_skip;
	/**The sampling rate of the original input.
	 All Opus audio is coded at 48 kHz, and should also be decoded at 48 kHz
	 for playback (unless the target hardware does not support this sampling
	 rate).
	 However, this field may be used to resample the audio back to the original
	 sampling rate, for example, when saving the output to a file.*/
	opus_uint32   input_sample_rate;
	/**The gain to apply to the decoded output, in dB, as a Q8 value in the range
	 -32768...32767.
	 The <tt>libopusfile</tt> API will automatically apply this gain to the
	 decoded output before returning it, scaling it by
	 <code>pow(10,output_gain/(20.0*256))</code>.
	 You can adjust this behavior with op_set_gain_offset().*/
	int           output_gain;
	/**The channel mapping family, in the range 0...255.
	 Channel mapping family 0 covers mono or stereo in a single stream.
	 Channel mapping family 1 covers 1 to 8 channels in one or more streams,
	 using the Vorbis speaker assignments.
	 Channel mapping family 255 covers 1 to 255 channels in one or more
	 streams, but without any defined speaker assignment.*/
	int           mapping_family;
	/**The number of Opus streams in each Ogg packet, in the range 1...255.*/
	int           stream_count;
	/**The number of coupled Opus streams in each Ogg packet, in the range
	 0...127.
	 This must satisfy <code>0 <= coupled_count <= stream_count</code> and
	 <code>coupled_count + stream_count <= 255</code>.
	 The coupled streams appear first, before all uncoupled streams, in an Ogg
	 Opus packet.*/
	int           coupled_count;
	/**The mapping from coded stream channels to output channels.
	 Let <code>index=mapping[k]</code> be the value for channel <code>k</code>.
	 If <code>index<2*coupled_count</code>, then it refers to the left channel
	 from stream <code>(index/2)</code> if even, and the right channel from
	 stream <code>(index/2)</code> if odd.
	 Otherwise, it refers to the output of the uncoupled stream
	 <code>(index-coupled_count)</code>.*/
	unsigned char mapping[OPUS_CHANNEL_COUNT_MAX];
};

struct opus_tags
{
	char **user_comments;	/**The array of comment string vectors.*/
	int   *comment_lengths;	/**An array of the corresponding length of each vector, in bytes.*/
	int    comments;		/**The total number of comment streams.*/
	char  *vendor;			/**The null-terminated vendor string. This identifies the software used to encode the stream.*/
};

enum opus_decode_state
{
	OPUS_STATE_INPUT,
	OPUS_STATE_DECODE
};


struct opus_lib
{
	ogg_sync_state	 oy;	/*Used to locate pages in the stream.*/
	ogg_stream_state os;	/*Takes physical pages and welds them into a logical stream of packets.*/

	ogg_page		 og;	/* one Ogg bitstream page. Vorbis packets are inside */
	ogg_packet		 op;

	OpusMSDecoder 	*od;		/*Central working state for the packet-to-PCM decoder.*/

	struct opus_head opus_head;
	struct opus_tags opus_tags;

	opus_int16 *od_buf;
	int od_buf_size;
	int od_buf_pos;

	int opus_header_init_flag;

	unsigned int* left_pcm_buf;
	unsigned int* right_pcm_buf;
	unsigned int sample_num;
	unsigned int eos;
	unsigned int consume_size;

	enum opus_decode_state decode_state;
} g_opus_lib;

static unsigned op_parse_uint16le(const unsigned char *_data)
{
	return _data[0] | _data[1] << 8;
}

static int op_parse_int16le(const unsigned char *_data)
{
	int ret;
	ret = _data[0] | _data[1] << 8;
	return (ret ^ 0x8000) - 0x8000;
}

static opus_uint32 op_parse_uint32le(const unsigned char *_data)
{
	return _data[0] | (opus_uint32)_data[1] << 8 |
	       (opus_uint32)_data[2] << 16 | (opus_uint32)_data[3] << 24;
}

int opus_head_parse(struct opus_head*_head, unsigned char *_data, int _len)
{
	struct opus_head head;

	if ((NULL == _head) || (NULL == _data) || (_len <= 0))
	{
		printf("<%s:%d> invalid parameter [%p/%p/%d]\n", __func__, __LINE__, _head, _data, _len);
		return -1;
	}

	if (_len < 8)
	{
		return -1;
	}

	if (memcmp(_data, "OpusHead", 8) != 0)
	{
		return -1;
	}

	if (_len < 9)
	{
		return -1;
	}

	head.version = _data[8];
	if (head.version > 15)
	{
		return -1;
	}

	if (_len < 19)
	{
		return -1;
	}

	head.channel_count = _data[9];
	head.pre_skip = (unsigned int)op_parse_uint16le(_data + 10);
	head.input_sample_rate = op_parse_uint32le(_data + 12);
	head.output_gain = op_parse_int16le(_data + 16);
	head.mapping_family = _data[18];
	if (head.mapping_family == 0)
	{
		if (head.channel_count < 1 || head.channel_count > 2)
		{
			return -1;
		}
		if (head.version <= 1 && _len > 19)
		{
			return -1;
		}
		head.stream_count = 1;
		head.coupled_count = head.channel_count - 1;
		if (_head != NULL)
		{
			_head->mapping[0] = 0;
			_head->mapping[1] = 1;
		}
	}
	else if (head.mapping_family == 1)
	{
		size_t size;
		int    ci;
		if (head.channel_count < 1 || head.channel_count > 8)
		{
			return -1;
		}
		size = 21 + head.channel_count;
		if (_len < size || (head.version <= 1 && _len > size))
		{
			return -1;
		}
		head.stream_count = _data[19];
		if (head.stream_count < 1)
		{
			return -1;
		}

		head.coupled_count = _data[20];
		if (head.coupled_count > head.stream_count)
		{
			return -1;
		}

		for (ci = 0; ci < head.channel_count; ci++)
		{
			if (_data[21 + ci] >= head.stream_count + head.coupled_count
			        && _data[21 + ci] != 255)
			{
				return -1;
			}
		}

		if (_head != NULL)
		{
			memcpy(_head->mapping, _data + 21, head.channel_count);
		}
	}
	/*General purpose players should not attempt to play back content with
	   channel mapping family 255.*/
	else if (head.mapping_family == 255)
	{
		return -1;
	}
	/*No other channel mapping families are currently defined.*/
	else
	{
		return -1;
	}

	if (_head != NULL)
	{
		memcpy(_head, &head, head.mapping - (unsigned char *)&head);
	}

	return 0;
}


static char *op_strdup_with_len(const char *_s, int _len)
{
	size_t  size;
	char   *ret;

	size = sizeof(*ret) * (_len + 1);
	if (size < _len)
	{
		return NULL;
	}

	ret = (char *)_ogg_malloc(size);
	if (ret != NULL)
	{
		ret = (char *)memcpy(ret, _s, sizeof(*ret) * _len);
		ret[_len] = '\0';
	}

	return ret;
}

static int op_tags_ensure_capacity(struct opus_tags *_tags, int _ncomments)
{
	char   **user_comments;
	int     *comment_lengths;
	int      cur_ncomments;
	int   size;
	if (_ncomments >= (size_t)INT_MAX)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}

	size = sizeof(*_tags->comment_lengths) * (_ncomments + 1);
	if (size / sizeof(*_tags->comment_lengths) != _ncomments + 1)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}

	cur_ncomments = _tags->comments;
	/*We only support growing.
	  Trimming requires cleaning up the allocated strings in the old space, and
	   is best handled separately if it's ever needed.*/
	if (_ncomments < cur_ncomments)
	{
		printf("L%d %d/%d\n", __LINE__, _ncomments, cur_ncomments);
		return -1;
	}

	comment_lengths = (int *)_ogg_realloc(_tags->comment_lengths, size);
	if (comment_lengths == NULL)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}
	if (_tags->comment_lengths == NULL)
	{
		if (cur_ncomments != 0)
		{
			printf("L%d\n", __LINE__);
			return -1;
		}
		comment_lengths[cur_ncomments] = 0;
	}

	comment_lengths[_ncomments] = comment_lengths[cur_ncomments];
	_tags->comment_lengths = comment_lengths;
	size = sizeof(*_tags->user_comments) * (_ncomments + 1);
	if (size / sizeof(*_tags->user_comments) != _ncomments + 1)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}

	user_comments = (char **)_ogg_realloc(_tags->user_comments, size);
	if (user_comments == NULL)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}
	if (_tags->user_comments == NULL)
	{
		if (cur_ncomments != 0)
		{
			printf("L%d\n", __LINE__);
			return -1;
		}
		user_comments[cur_ncomments] = NULL;
	}
	user_comments[_ncomments] = user_comments[cur_ncomments];
	_tags->user_comments = user_comments;

	return 0;
}

void opus_tags_clear(struct opus_tags *_tags)
{
	int ncomments;
	int ci;
	ncomments = _tags->comments;

	if (_tags->user_comments != NULL)
	{
		ncomments++;
	}
	else
	{
		if (ncomments != 0)
		{
			return;
		}
	}

	for (ci = ncomments; ci-- > 0;)
	{
		if (_tags->user_comments)
		{
			if (_tags->user_comments[ci])
			{
				_ogg_free(_tags->user_comments[ci]);
				_tags->user_comments[ci] = NULL;
			}
		}
	}

	if (_tags->user_comments)
	{
		_ogg_free(_tags->user_comments);
		_tags->user_comments = NULL;
	}

	if (_tags->comment_lengths)
	{
		_ogg_free(_tags->comment_lengths);
		_tags->comment_lengths = NULL;
	}

	if (_tags->vendor)
	{
		_ogg_free(_tags->vendor);
		_tags->vendor = NULL;
	}
}

static int opus_tags_parse(struct opus_tags *_tags, unsigned char *_data, int _len)
{
	opus_uint32 count;
	int len;
	int ncomments;
	int ci;
	int ret;

	if ((NULL == _tags) || (NULL == _data) || (_len <= 0))
	{
		printf("<%s:%d> invalid parameter [%p/%p/%d]\n", __func__, __LINE__, _tags, _data, _len);
		return -1;
	}

	len = _len;
	if (len < 8)
	{
		printf("L%d len:%d\n", __LINE__, len);
		return -1;
	}

	if (memcmp(_data, "OpusTags", 8) != 0)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}

	if (len < 16)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}
	_data += 8;
	len -= 8;
	count = op_parse_uint32le(_data);
	_data += 4;
	len -= 4;
	if (count > len)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}

	_tags->vendor = op_strdup_with_len((char *)_data, count);
	if (_tags->vendor == NULL)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}

	_data += count;
	len -= count;
	if (len < 4)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}
	count = op_parse_uint32le(_data);
	_data += 4;
	len -= 4;
	/*Check to make sure there's minimally sufficient data left in the packet.*/
	if (count > len >> 2)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}
	/*Check for overflow (the API limits this to an int).*/
	if (count > (opus_uint32)INT_MAX - 1)
	{
		printf("L%d\n", __LINE__);
		return -1;
	}

	ret = op_tags_ensure_capacity(_tags, count);
	if (ret < 0)
	{
		printf("L%d\n", __LINE__);
		return ret;
	}

	ncomments = (int)count;
	for (ci = 0; ci < ncomments; ci++)
	{
		/*Check to make sure there's minimally sufficient data left in the packet.*/
		if ((ncomments - ci) > len >> 2)
		{
			printf("L%d\n", __LINE__);
			return -1;
		}
		count = op_parse_uint32le(_data);
		_data += 4;
		len -= 4;
		if (count > len)
		{
			printf("L%d\n", __LINE__);
			return -1;
		}
		/*Check for overflow (the API limits this to an int).*/
		if (count > (opus_uint32)INT_MAX)
		{
			printf("L%d\n", __LINE__);
			return -1;
		}
		if (_tags != NULL)
		{
			_tags->user_comments[ci] = op_strdup_with_len((char *)_data, count);
			if (_tags->user_comments[ci] == NULL)
			{
				printf("L%d\n", __LINE__);
				return -1;
			}
			_tags->comment_lengths[ci] = (int)count;
			_tags->comments = ci + 1;
			/*Needed by opus_tags_clear() if we fail before parsing the (optional)
			   binary metadata.*/
			_tags->user_comments[ci + 1] = NULL;
		}
		_data += count;
		len -= count;
	}

	if (len > 0 && (_data[0] & 1))
	{
		if (len > (opus_uint32)INT_MAX)
		{
			printf("L%d\n", __LINE__);
			return -1;
		}
		if (_tags != NULL)
		{
			_tags->user_comments[ncomments] = (char *)_ogg_malloc(len);
			if (_tags->user_comments[ncomments] == NULL)
			{
				printf("L%d\n", __LINE__);
				return -1;
			}
			memcpy(_tags->user_comments[ncomments], _data, len);
			_tags->comment_lengths[ncomments] = (int)len;
		}
	}

	return 0;
}

static int op_get_packet_duration(const unsigned char *_data, int _len)
{
	int nframes;
	int frame_size;
	int nsamples;

	nframes = opus_packet_get_nb_frames(_data, _len);
	//printf("<%s:%d> opus_packet_get_nb_frames() nframes:%d\n", __func__, __LINE__, nframes);
	if (nframes < 0)
	{
		return -1;
	}

	frame_size = opus_packet_get_samples_per_frame(_data, 48000);	// ????????
	//printf("<%s:%d> opus_packet_get_samples_per_frame() frame_size:%d\n", __func__, __LINE__, frame_size);
	nsamples = nframes * frame_size;
	if (nsamples > 120 * 48)
	{
		return -1;
	}

	return nsamples;
}

int opus_init(struct opus_lib* opus_lib)
{
	if (NULL == opus_lib)
	{
		printf("<%s:%d> Invalid parameter!\n", __func__, __LINE__);
		return -1;
	}

	opus_lib->sample_num = 0;
	opus_lib->decode_state = OPUS_STATE_INPUT;

	memset(&opus_lib->opus_tags, 0, sizeof(opus_lib->opus_tags));

	ogg_sync_init(&opus_lib->oy);
	opus_lib->opus_header_init_flag = 0;

	opus_lib->od_buf_pos = 0;
	opus_lib->od_buf_size = sizeof(opus_int16) * 2 * 120 * 48;
	opus_lib->od_buf = malloc(opus_lib->od_buf_size);	// for 2 channel pcm data
	if (NULL == opus_lib->od_buf)
	{
		printf("<%s:%d> alloc od_buf error!\n", __func__, __LINE__);
		return -1;
	}

	//opus_lib->left_pcm_buf = dl_malloc(OPUS_PCM_BUF_SIZE);	// ??????
	opus_lib->left_pcm_buf = malloc(OPUS_PCM_BUF_SIZE);	// ??????
	if (NULL == opus_lib->left_pcm_buf)
	{
		//FATAL_ERROR("<ah><%s:%d> malloc left_pcm_buf error!\n", __func__, __LINE__);
		//return HA_ErrorInvalidParameter;

		printf("<ah><%s:%d> malloc left_pcm_buf error!\n", __func__, __LINE__);
		return -1;
	}

	//opus_lib->right_pcm_buf = dl_malloc(OPUS_PCM_BUF_SIZE);
	opus_lib->right_pcm_buf = malloc(OPUS_PCM_BUF_SIZE);
	if (NULL == opus_lib->right_pcm_buf)
	{
		//FATAL_ERROR("<ah><%s:%d> malloc right_pcm_buf error!\n", __func__, __LINE__);
		//dl_free(opus_lib->left_pcm_buf);
		//return HA_ErrorInvalidParameter;

		printf("<ah><%s:%d> malloc right_pcm_buf error!\n", __func__, __LINE__);
		free(opus_lib->left_pcm_buf);
		return -1;
	}

	return 0;
}

int opus_release(struct opus_lib* opus_lib)
{
	if (NULL == opus_lib)
	{
		printf("<%s:%d> Invalid parameter!\n", __func__, __LINE__);
		return -1;
	}

	ogg_stream_clear(&opus_lib->os);

	/* OK, clean up the framer */
	ogg_sync_clear(&opus_lib->oy);

	opus_tags_clear(&opus_lib->opus_tags);

	opus_multistream_decoder_destroy(opus_lib->od);

	if (opus_lib->od_buf)
	{
		free(opus_lib->od_buf);
		opus_lib->od_buf = NULL;
	}

	if (opus_lib->left_pcm_buf)
	{
		free(opus_lib->left_pcm_buf);
		opus_lib->left_pcm_buf = NULL;
	}

	if (opus_lib->right_pcm_buf)
	{
		free(opus_lib->right_pcm_buf);
		opus_lib->right_pcm_buf = NULL;
	}

	return 0;
}

static int opus_header_init(struct opus_lib* opus_lib)
{
	int ret;
	if (NULL == opus_lib)
	{
		printf("<%s:%d> Invalid parameter!\n", __func__, __LINE__);
		return -1;
	}

	/*opus head*/
	if (0 == opus_lib->opus_header_init_flag)
	{
		ret = ogg_sync_pageout(&opus_lib->oy, &opus_lib->og);
		if (0 == ret)
		{
			printf("L%d need more data!\n", __LINE__);
			return 1;
		}

		if (1 != ret)
		{
			printf("L%d ogg_sync_pageout error!\n", __LINE__);
			return -1;
		}

		/**
		Get the serial number and set up the rest of decode
		serialno first; use it to set up a logical stream
		*/
		ogg_stream_init(&opus_lib->os, ogg_page_serialno(&opus_lib->og));

		if (ogg_stream_pagein(&opus_lib->os, &opus_lib->og) < 0)
		{
			/* error; stream version mismatch perhaps */
			printf("Error reading first page of Ogg bitstream data.\n");
			return -1;
		}

		if (ogg_stream_packetout(&opus_lib->os, &opus_lib->op) != 1)
		{
			/* no page? must not be vorbis */
			printf("Error reading initial header packet.\n");
			return -1;
		}

		ret = opus_head_parse(&opus_lib->opus_head, opus_lib->op.packet, opus_lib->op.bytes);
		if (ret >= 0)
		{
			opus_lib->opus_header_init_flag = 1;

			printf("version:%d\n", opus_lib->opus_head.version);
			printf("ch:%d\n", opus_lib->opus_head.channel_count);
			printf("pre_skip:%d\n", opus_lib->opus_head.pre_skip);
			printf("rate:%d\n", opus_lib->opus_head.input_sample_rate);
			printf("output_gain:%d\n", opus_lib->opus_head.output_gain);
			printf("mapping_family:%d\n", opus_lib->opus_head.mapping_family);
			printf("stream_count:%d\n", opus_lib->opus_head.stream_count);
			printf("coupled_count:%d\n", opus_lib->opus_head.coupled_count);
		}
	}

	/*opus tag*/
	if (1 == opus_lib->opus_header_init_flag)
	{
		ret = ogg_sync_pageout(&opus_lib->oy, &opus_lib->og);
		if (0 == ret)
		{
			printf("L%d need more data!\n", __LINE__);
			return 1;
		}

		if (1 != ret)
		{
			printf("L%d ogg_sync_pageout error!\n", __LINE__);
			return -1;
		}

		/*ret = ogg_sync_pageseek(&opus_lib->oy, &opus_lib->og);
		printf("L%d ret:%d\n", __LINE__, ret);
		if (0 == ret)
		{
			printf("L%d need more data!\n", __LINE__);
			return 1;
		}*/

		if (ogg_stream_pagein(&opus_lib->os, &opus_lib->og) < 0)
		{
			/* error; stream version mismatch perhaps */
			printf("L%d Error reading first page of Ogg bitstream data.\n", __LINE__);
			return -1;
		}

		if (ogg_stream_packetout(&opus_lib->os, &opus_lib->op) != 1)
		{
			/* no page? must not be vorbis */
			printf("L%d Error reading initial header packet.\n", __LINE__);
			return -1;
		}

		printf("L%d byte:%ld\n", __LINE__, opus_lib->op.bytes);

		ret = opus_tags_parse(&opus_lib->opus_tags, opus_lib->op.packet, opus_lib->op.bytes);
		if (ret < 0)
		{
			printf("L%d opus_tags_parse error, ret:%d\n", __LINE__, ret);
			opus_tags_clear(&opus_lib->opus_tags);
		}
		else
		{
			printf("Encoded by: %s\n", opus_lib->opus_tags.vendor);
			opus_lib->opus_header_init_flag = 2;

			struct opus_head* head = &opus_lib->opus_head;
			opus_lib->od = opus_multistream_decoder_create(48000, head->channel_count,
			                                               head->stream_count, head->coupled_count, head->mapping, &ret);
			if (opus_lib->od == NULL)
			{
				printf("L%d opus_multistream_decoder_create error! ret:%d\n", __LINE__, ret);
				return -1;
			}
		}
	}

	return 0;
}

static int convert_pcm_data(struct opus_lib* opus_lib, int sample_num)
{
	int i;
	int channel;
	unsigned int *left_ch = NULL;
	unsigned int *right_ch = NULL;

	if ((NULL == opus_lib) || (sample_num <= 0))
	{
		printf("<%s:%d>Invalid parameter!\n", __func__, __LINE__);
		return -1;
	}

	// check pcm buf left space
	if (opus_lib->sample_num + sample_num > OPUS_SAMPLES_IN_BUF)
	{
		printf("pcm buf too small, discard %d samples!\n", opus_lib->sample_num + sample_num - OPUS_SAMPLES_IN_BUF);
		sample_num = OPUS_SAMPLES_IN_BUF - opus_lib->sample_num;
	}

	left_ch = opus_lib->left_pcm_buf + opus_lib->sample_num;
	right_ch = opus_lib->right_pcm_buf + opus_lib->sample_num;

	channel = opus_lib->opus_head.channel_count;
	if (1 == channel)
	{
		for (i = 0; i < sample_num; i++)
		{
			left_ch[i] = right_ch[i] = opus_lib->od_buf[i];
		}
	}
	else if (2 == channel)
	{
		for (i = 0; i < sample_num; i++)
		{
			left_ch[i] = opus_lib->od_buf[i << 1];
			right_ch[i] = opus_lib->od_buf[(i << 1) + 1];
		}
		//fwrite(opus_lib->left_pcm_buf, 1, sample_num*4, fout);
		//fwrite(opus_lib->right_pcm_buf, 1, sample_num*4, fout);
	}
	else
	{
		for (i = 0; i < sample_num; i++)
		{
			opus_int32 l;
			opus_int32 r;
			int        ci;
			l = r = 0;
			for (ci = 0; ci < channel; ci++)
			{
				opus_int32 s;
				s = opus_lib->od_buf[channel * i + ci];
				l += OP_STEREO_DOWNMIX_Q14[channel - 3][ci][0] * s;
				r += OP_STEREO_DOWNMIX_Q14[channel - 3][ci][1] * s;
			}
			/*TODO: For 5 or more channels, we should do soft clipping here.*/
			left_ch[i] = (opus_int16)OP_CLAMP(-32768, l + 8192 >> 14, 32767);
			right_ch[i] = (opus_int16)OP_CLAMP(-32768, r + 8192 >> 14, 32767);
		}
	}

	opus_lib->sample_num += sample_num;

	return 0;
}

static int opus_input_data(struct opus_lib* opus_lib, char* src, int size)
{
	char *buffer;
	int ret;

	if ((NULL == opus_lib) || (NULL == src) || (size <= 0))
	{
		printf("<%s:%d>Invalid parameter [%p/%p/%d]\n", __func__, __LINE__, opus_lib, src, size);
		return -1;
	}

	if (OPUS_STATE_INPUT != g_opus_lib.decode_state)
	{
		return -1;
	}

	buffer = ogg_sync_buffer(&opus_lib->oy, size);
	if (buffer)
	{
		memcpy(buffer, src, size);
	}
	else
	{
		printf("L:%d ogg_sync_buffer return NULL!\n", __LINE__);
		return -1;
	}
	ogg_sync_wrote(&opus_lib->oy, size);

	if (2 != opus_lib->opus_header_init_flag)
	{
		ret = opus_header_init(opus_lib);
		if (0 != ret)
		{
			return ret;
		}
	}

	ret = ogg_sync_pageout(&opus_lib->oy, &opus_lib->og);
	if (ret == 0)
	{
		return 1;	 /* need more data */
	}

	if (ret < 0) /* missing or corrupt data at this page position */
	{
		//FATAL_ERROR("Corrupt or missing data in bitstream; continuing...\n");
		return -1;
	}
	else
	{
		ogg_stream_pagein(&opus_lib->os, &opus_lib->og); /* can safely ignore errors at this point */
		opus_lib->decode_state = OPUS_STATE_DECODE;
	}

	return 0;
}

static int opus_decode1(struct opus_lib* opus_lib)
{
	int ret;

	if (NULL == opus_lib)
	{
		printf("<%s:%d>Invalid parameter [%p]\n", __func__, __LINE__, opus_lib);
		return -1;
	}

	if (OPUS_STATE_DECODE != opus_lib->decode_state)
	{
		return -1;
	}

	ret = ogg_stream_packetout(&opus_lib->os, &opus_lib->op);
	if (ret == 0)
	{
		opus_lib->decode_state = OPUS_STATE_INPUT;
		return 1;    /* need more data */
	}

	if (ret < 0) /* missing or corrupt data at this page position */
	{
		/* no reason to complain; already complained above */
	}
	else
	{
		// start decode
		int duration = op_get_packet_duration(opus_lib->op.packet, opus_lib->op.bytes);
		ret = opus_multistream_decode(opus_lib->od, opus_lib->op.packet,
		                              opus_lib->op.bytes, opus_lib->od_buf, duration, 0);
		if (ret < 0)
		{
			printf("L%d decode error!\n", __LINE__);
			return ret;
		}

		// convert pcm data
		convert_pcm_data(opus_lib, ret);
		//printf("L%d sample_num:%d\n", __LINE__, opus_lib->sample_num);
	}

	if (ogg_page_eos(&opus_lib->og))
	{
		//FATAL_ERROR("L%d ogg file eos!\n", __LINE__);
		printf("L%d opus file eos!\n", __LINE__);
		opus_lib->eos = 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	FILE *fin = NULL;
	FILE *fout = NULL;
	char src[4096];
	int read_size = 0, ret;
	int end = 0;

	if (argc < 3)
	{
		printf("usage %s <input.opus> <output.wav>\n", argv[0]);
		return -1;
	}

	fin = fopen(argv[1], "r");
	if (NULL == fin)
	{
		printf("open %s error!\n", argv[1]);
		return -1;
	}

	fout = fopen(argv[2], "w+");
	if (NULL == fout)
	{
		printf("open %s error!\n", argv[2]);
		fclose(fin);
		return -1;
	}

	opus_init(&g_opus_lib);

	while (!end)
	{
		while (OPUS_STATE_INPUT == g_opus_lib.decode_state)
		{
			read_size = fread(src, 1, 4096, fin);
			if (read_size == 0)
			{
				printf("read file end!\n");
				end = 1;
				break;
			}

			ret = opus_input_data(&g_opus_lib, src, read_size);
			if (ret < 0)
			{
				printf("L%d opus_input_data error!\n", __LINE__);
			}
		}

		if (OPUS_STATE_DECODE == g_opus_lib.decode_state)
		{
			//printf("L%d read:%d\n", __LINE__, read_size);
			ret = opus_decode1(&g_opus_lib);
			if (ret < 0)
			{
				printf("L%d decode error!\n", __LINE__);
			}

			//printf("L%d sample_num:%d\n", __LINE__, g_opus_lib.sample_num);
			if (g_opus_lib.sample_num > 0)
			{
				//fwrite(g_opus_lib.left_pcm_buf, 1, g_opus_lib.sample_num*4, fout);
				fwrite(g_opus_lib.right_pcm_buf, 1, g_opus_lib.sample_num * 4, fout);

				g_opus_lib.sample_num = 0;
			}
		}
	}

	opus_release(&g_opus_lib);

	fclose(fin);
	fclose(fout);

	return 0;
}
