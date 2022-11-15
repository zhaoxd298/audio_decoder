//
//  latm2adts.c
//  LATM2ADTS
//
//  Created by mini on 2017/5/19.
//  Copyright © 2017年 mini. All rights reserved.
//

#include "latm2adts.h"
#include <stddef.h>
#include <assert.h>
#include <limits.h>

#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "get_bits.h"
#include "put_bits.h"


static const int8_t tags_per_config[16] = { 0, 1, 1, 2, 3, 3, 4, 5, 0, 0, 0, 5, 5, 16, 5, 0 };

static const uint8_t aac_channel_layout_map[16][16][3] =
{
	{ { TYPE_SCE, 0, AAC_CHANNEL_FRONT }, },
	{ { TYPE_CPE, 0, AAC_CHANNEL_FRONT }, },
	{ { TYPE_SCE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 0, AAC_CHANNEL_FRONT }, },
	{ { TYPE_SCE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 0, AAC_CHANNEL_FRONT }, { TYPE_SCE, 1, AAC_CHANNEL_BACK }, },
	{ { TYPE_SCE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 1, AAC_CHANNEL_BACK }, },
	{ { TYPE_SCE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 1, AAC_CHANNEL_BACK }, { TYPE_LFE, 0, AAC_CHANNEL_LFE  }, },
	{ { TYPE_SCE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 1, AAC_CHANNEL_FRONT }, { TYPE_CPE, 2, AAC_CHANNEL_BACK }, { TYPE_LFE, 0, AAC_CHANNEL_LFE  }, },
	{ { 0, } },
	{ { 0, } },
	{ { 0, } },
	{ { TYPE_SCE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 1, AAC_CHANNEL_BACK }, { TYPE_SCE, 1, AAC_CHANNEL_BACK }, { TYPE_LFE, 0, AAC_CHANNEL_LFE  }, },
	{ { TYPE_SCE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 1, AAC_CHANNEL_BACK }, { TYPE_CPE, 2, AAC_CHANNEL_BACK }, { TYPE_LFE, 0, AAC_CHANNEL_LFE  }, },
	{
		{ TYPE_SCE, 0, AAC_CHANNEL_FRONT }, // SCE1 = FC,
		{ TYPE_CPE, 0, AAC_CHANNEL_FRONT }, // CPE1 = FLc and FRc,
		{ TYPE_CPE, 1, AAC_CHANNEL_FRONT }, // CPE2 = FL and FR,
		{ TYPE_CPE, 2, AAC_CHANNEL_BACK  }, // CPE3 = SiL and SiR,
		{ TYPE_CPE, 3, AAC_CHANNEL_BACK  }, // CPE4 = BL and BR,
		{ TYPE_SCE, 1, AAC_CHANNEL_BACK  }, // SCE2 = BC,
		{ TYPE_LFE, 0, AAC_CHANNEL_LFE   }, // LFE1 = LFE1,
		{ TYPE_LFE, 1, AAC_CHANNEL_LFE   }, // LFE2 = LFE2,
		{ TYPE_SCE, 2, AAC_CHANNEL_FRONT }, // SCE3 = TpFC,
		{ TYPE_CPE, 4, AAC_CHANNEL_FRONT }, // CPE5 = TpFL and TpFR,
		{ TYPE_CPE, 5, AAC_CHANNEL_SIDE  }, // CPE6 = TpSiL and TpSiR,
		{ TYPE_SCE, 3, AAC_CHANNEL_SIDE  }, // SCE4 = TpC,
		{ TYPE_CPE, 6, AAC_CHANNEL_BACK  }, // CPE7 = TpBL and TpBR,
		{ TYPE_SCE, 4, AAC_CHANNEL_BACK  }, // SCE5 = TpBC,
		{ TYPE_SCE, 5, AAC_CHANNEL_FRONT }, // SCE6 = BtFC,
		{ TYPE_CPE, 7, AAC_CHANNEL_FRONT }, // CPE8 = BtFL and BtFR
	},
	{ { TYPE_SCE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 0, AAC_CHANNEL_FRONT }, { TYPE_CPE, 1, AAC_CHANNEL_BACK }, { TYPE_LFE, 0, AAC_CHANNEL_LFE }, { TYPE_CPE, 2, AAC_CHANNEL_FRONT  }, },
	{ { 0, } },
};

static const int16_t aac_channel_map[3][4][6] =
{
	{
		{ AV_CHAN_FRONT_CENTER,        AV_CHAN_FRONT_LEFT_OF_CENTER, AV_CHAN_FRONT_RIGHT_OF_CENTER, AV_CHAN_FRONT_LEFT,        AV_CHAN_FRONT_RIGHT,        AV_CHAN_NONE },
		{ AV_CHAN_UNUSED,              AV_CHAN_NONE,                 AV_CHAN_NONE,                  AV_CHAN_NONE,              AV_CHAN_NONE,               AV_CHAN_NONE },
		{ AV_CHAN_UNUSED,              AV_CHAN_SIDE_LEFT,            AV_CHAN_SIDE_RIGHT,            AV_CHAN_BACK_LEFT,         AV_CHAN_BACK_RIGHT,         AV_CHAN_BACK_CENTER },
		{ AV_CHAN_LOW_FREQUENCY,       AV_CHAN_LOW_FREQUENCY_2,      AV_CHAN_NONE,                  AV_CHAN_NONE,              AV_CHAN_NONE,               AV_CHAN_NONE },
	},
	{
		{ AV_CHAN_TOP_FRONT_CENTER,    AV_CHAN_NONE,                 AV_CHAN_NONE,                  AV_CHAN_TOP_FRONT_LEFT,    AV_CHAN_TOP_FRONT_RIGHT,    AV_CHAN_NONE },
		{ AV_CHAN_UNUSED,              AV_CHAN_TOP_SIDE_LEFT,        AV_CHAN_TOP_SIDE_RIGHT,        AV_CHAN_NONE,              AV_CHAN_NONE,               AV_CHAN_TOP_CENTER},
		{ AV_CHAN_UNUSED,              AV_CHAN_NONE,                 AV_CHAN_NONE,                  AV_CHAN_TOP_BACK_LEFT,     AV_CHAN_TOP_BACK_RIGHT,     AV_CHAN_TOP_BACK_CENTER},
		{ AV_CHAN_NONE,                AV_CHAN_NONE,                 AV_CHAN_NONE,                  AV_CHAN_NONE,              AV_CHAN_NONE,               AV_CHAN_NONE},
	},
	{
		{ AV_CHAN_BOTTOM_FRONT_CENTER, AV_CHAN_NONE,                 AV_CHAN_NONE,                  AV_CHAN_BOTTOM_FRONT_LEFT, AV_CHAN_BOTTOM_FRONT_RIGHT, AV_CHAN_NONE },
		{ AV_CHAN_NONE,                AV_CHAN_NONE,                 AV_CHAN_NONE,                  AV_CHAN_NONE,              AV_CHAN_NONE,               AV_CHAN_NONE },
		{ AV_CHAN_NONE,                AV_CHAN_NONE,                 AV_CHAN_NONE,                  AV_CHAN_NONE,              AV_CHAN_NONE,               AV_CHAN_NONE },
		{ AV_CHAN_NONE,                AV_CHAN_NONE,                 AV_CHAN_NONE,                  AV_CHAN_NONE,              AV_CHAN_NONE,               AV_CHAN_NONE },
	},
};

const uint8_t ff_mpeg4audio_channels[15] =
{
	0,
	1, // mono (1/0)
	2, // stereo (2/0)
	3, // 3/0
	4, // 3/1
	5, // 3/2
	6, // 3/2.1
	8, // 5/2.1
	0,
	0,
	0,
	7, // 3/3.1
	8, // 3/2/2.1
	24, // 3/3/3 - 5/2/3 - 3/0/0.2
	8, // 3/2.1 - 2/0
};

const int ff_mpeg4audio_sample_rates[16] =
{
	96000, 88200, 64000, 48000, 44100, 32000,
	24000, 22050, 16000, 12000, 11025, 8000, 7350
};

static inline uint32_t latm_get_value(GetBitContext *b)
{
	int length = get_bits(b, 2);

	return get_bits_long(b, (length + 1) * 8);
}

/**
 * Decode an array of 4 bit element IDs, optionally interleaved with a
 * stereo/mono switching bit.
 *
 * @param type speaker type/position for these channels
 */
static void decode_channel_map(uint8_t layout_map[][3],
                               enum ChannelPosition type,
                               GetBitContext *gb, int n)
{
	while (n--)
	{
		enum RawDataBlockType syn_ele;
		switch (type)
		{
		case AAC_CHANNEL_FRONT:
		case AAC_CHANNEL_BACK:
		case AAC_CHANNEL_SIDE:
			syn_ele = get_bits1(gb);
			break;
		case AAC_CHANNEL_CC:
			skip_bits1(gb);
			syn_ele = TYPE_CCE;
			break;
		case AAC_CHANNEL_LFE:
			syn_ele = TYPE_LFE;
			break;
		default:
			// AAC_CHANNEL_OFF has no channel map
			break;
		}
		layout_map[0][0] = syn_ele;
		layout_map[0][1] = get_bits(gb, 4);
		layout_map[0][2] = type;
		layout_map++;
	}
}

static inline void relative_align_get_bits(GetBitContext *gb,
                                           int reference_position)
{
	int n = (reference_position - get_bits_count(gb) & 7);
	if (n)
	{
		skip_bits(gb, n);
	}
}

/**
 * Decode program configuration element; reference: table 4.2.
 *
 * @return  Returns error status. 0 - OK, !0 - error
 */
static int decode_pce(MPEG4AudioConfig *m4ac,
                      uint8_t (*layout_map)[3],
                      GetBitContext *gb, int byte_align_ref)
{
	int num_front, num_side, num_back, num_lfe, num_assoc_data, num_cc;
	int sampling_index;
	int comment_len;
	int tags;

	skip_bits(gb, 2);  // object_type

	sampling_index = get_bits(gb, 4);
	if (m4ac->sampling_index != sampling_index)
		printf("Sample rate index in program config element does not "
		       "match the sample rate index configured by the container.\n");

	num_front       = get_bits(gb, 4);
	num_side        = get_bits(gb, 4);
	num_back        = get_bits(gb, 4);
	num_lfe         = get_bits(gb, 2);
	num_assoc_data  = get_bits(gb, 3);
	num_cc          = get_bits(gb, 4);

	if (get_bits1(gb))
	{
		skip_bits(gb, 4);    // mono_mixdown_tag
	}
	if (get_bits1(gb))
	{
		skip_bits(gb, 4);    // stereo_mixdown_tag
	}

	if (get_bits1(gb))
	{
		skip_bits(gb, 3);    // mixdown_coeff_index and pseudo_surround
	}

	if (get_bits_left(gb) < 5 * (num_front + num_side + num_back + num_cc) + 4 * (num_lfe + num_assoc_data + num_cc))
	{
		printf("decode_pce L%d\n", __LINE__);
		return -1;
	}
	decode_channel_map(layout_map, AAC_CHANNEL_FRONT, gb, num_front);
	tags = num_front;
	decode_channel_map(layout_map + tags, AAC_CHANNEL_SIDE,  gb, num_side);
	tags += num_side;
	decode_channel_map(layout_map + tags, AAC_CHANNEL_BACK,  gb, num_back);
	tags += num_back;
	decode_channel_map(layout_map + tags, AAC_CHANNEL_LFE,   gb, num_lfe);
	tags += num_lfe;

	skip_bits_long(gb, 4 * num_assoc_data);

	decode_channel_map(layout_map + tags, AAC_CHANNEL_CC,    gb, num_cc);
	tags += num_cc;

	relative_align_get_bits(gb, byte_align_ref);

	/* comment field, first byte is length */
	comment_len = get_bits(gb, 8) * 8;
	if (get_bits_left(gb) < comment_len)
	{
		printf("decode_pce L%d\n", __LINE__);
		return -1;
	}
	skip_bits_long(gb, comment_len);
	return tags;
}

static int count_channels(uint8_t (*layout)[3], int tags)
{
	int i, sum = 0;
	for (i = 0; i < tags; i++)
	{
		int syn_ele = layout[i][0];
		int pos     = layout[i][2];
		sum += (1 + (syn_ele == TYPE_CPE)) *
		       (pos != AAC_CHANNEL_OFF && pos != AAC_CHANNEL_CC);
	}
	return sum;
}

/**
 * Set up channel positions based on a default channel configuration
 * as specified in table 1.17.
 *
 * @return  Returns error status. 0 - OK, !0 - error
 */
static int set_default_channel_config(uint8_t (*layout_map)[3],
                                      int *tags,
                                      int channel_config)
{
	if (channel_config < 1 || (channel_config > 7 && channel_config < 11) ||
	        channel_config > 14)
	{
		printf("invalid default channel configuration (%d)\n", channel_config);
		return -1;
	}
	*tags = tags_per_config[channel_config];
	memcpy(layout_map, aac_channel_layout_map[channel_config - 1], *tags * sizeof(*layout_map));

	/*
	 * AAC specification has 7.1(wide) as a default layout for 8-channel streams.
	 * However, at least Nero AAC encoder encodes 7.1 streams using the default
	 * channel config 7, mapping the side channels of the original audio stream
	 * to the second AAC_CHANNEL_FRONT pair in the AAC stream. Similarly, e.g. FAAD
	 * decodes the second AAC_CHANNEL_FRONT pair as side channels, therefore decoding
	 * the incorrect streams as if they were correct (and as the encoder intended).
	 *
	 * As actual intended 7.1(wide) streams are very rare, default to assuming a
	 * 7.1 layout was intended.
	 */
	/*	todo
	if (channel_config == 7 && avctx->strict_std_compliance < FF_COMPLIANCE_STRICT) {
	    layout_map[2][2] = AAC_CHANNEL_BACK;

	    if (!ac || !ac->warned_71_wide++) {
	        av_log(avctx, AV_LOG_INFO, "Assuming an incorrectly encoded 7.1 channel layout"
	               " instead of a spec-compliant 7.1(wide) layout, use -strict %d to decode"
	               " according to the specification instead.\n", FF_COMPLIANCE_STRICT);
	    }
	}*/

	return 0;
}

/**
 * Decode GA "General Audio" specific configuration; reference: table 4.1.
 *
 * @param   ac          pointer to AACContext, may be null
 * @param   avctx       pointer to AVCCodecContext, used for logging
 *
 * @return  Returns error status. 0 - OK, !0 - error
 */
static int decode_ga_specific_config(GetBitContext *gb,
                                     int get_bit_alignment,
                                     MPEG4AudioConfig *m4ac,
                                     int channel_config)
{
	int extension_flag, ret, ep_config, res_flags;
	uint8_t layout_map[MAX_ELEM_ID * 4][3];
	int tags = 0;

	m4ac->frame_length_short = get_bits1(gb);
	if (m4ac->frame_length_short && m4ac->sbr == 1)
	{
		m4ac->sbr = 0;
		m4ac->ps = 0;
	}

	if (get_bits1(gb))       // dependsOnCoreCoder
	{
		skip_bits(gb, 14);    // coreCoderDelay
	}
	extension_flag = get_bits1(gb);

	if (m4ac->object_type == AOT_AAC_SCALABLE ||
	        m4ac->object_type == AOT_ER_AAC_SCALABLE)
	{
		skip_bits(gb, 3);    // layerNr
	}

	if (channel_config == 0)
	{
		skip_bits(gb, 4);  // element_instance_tag
		tags = decode_pce(m4ac, layout_map, gb, get_bit_alignment);
		if (tags < 0)
		{
			return tags;
		}
	}
	else
	{

		if ((ret = set_default_channel_config(layout_map,
		                                      &tags, channel_config)))
		{
			return ret;
		}
	}

	if (count_channels(layout_map, tags) > 1)
	{
		m4ac->ps = 0;
	}
	else if (m4ac->sbr == 1 && m4ac->ps == -1)
	{
		m4ac->ps = 1;
	}
	/*
	if (ac && (ret = output_configure(ac, layout_map, tags, OC_GLOBAL_HDR, 0)))
	    return ret;
	*/
	if (extension_flag)
	{
		switch (m4ac->object_type)
		{
		case AOT_ER_BSAC:
			skip_bits(gb, 5);    // numOfSubFrame
			skip_bits(gb, 11);   // layer_length
			break;
		case AOT_ER_AAC_LC:
		case AOT_ER_AAC_LTP:
		case AOT_ER_AAC_SCALABLE:
		case AOT_ER_AAC_LD:
			res_flags = get_bits(gb, 3);
			if (res_flags)
			{
				printf("AAC data resilience (flags %x)", res_flags);
				return -1;
			}
			break;
		}
		skip_bits1(gb);    // extensionFlag3 (TBD in version 3)
	}
	switch (m4ac->object_type)
	{
	case AOT_ER_AAC_LC:
	case AOT_ER_AAC_LTP:
	case AOT_ER_AAC_SCALABLE:
	case AOT_ER_AAC_LD:
		ep_config = get_bits(gb, 2);
		if (ep_config)
		{
			printf("epConfig %d", ep_config);
			return -1;
		}
	}
	return 0;
}

static inline int get_object_type(GetBitContext *gb)
{
	int object_type = get_bits(gb, 5);
	if (object_type == AOT_ESCAPE)
	{
		object_type = 32 + get_bits(gb, 6);
	}
	return object_type;
}

static inline int get_sample_rate(GetBitContext *gb, int *index)
{
	*index = get_bits(gb, 4);
	return *index == 0x0f ? get_bits(gb, 24) :
	       ff_mpeg4audio_sample_rates[*index];
}

/**
 * Parse MPEG-4 audio configuration for ALS object type.
 * @param[in] gb       bit reader context
 * @param[in] c        MPEG4AudioConfig structure to fill
 * @return on success 0 is returned, otherwise a value < 0
 */
static int parse_config_ALS(GetBitContext *gb, MPEG4AudioConfig *c)
{
	if (get_bits_left(gb) < 112)
	{
		return -1;
	}

	if (get_bits_long(gb, 32) != MKBETAG('A', 'L', 'S', '\0'))
	{
		return -1;
	}

	// override AudioSpecificConfig channel configuration and sample rate
	// which are buggy in old ALS conformance files
	c->sample_rate = get_bits_long(gb, 32);

	if (c->sample_rate <= 0)
	{
		printf("Invalid sample rate %d\n", c->sample_rate);
		return -1;
	}

	// skip number of samples
	skip_bits_long(gb, 32);

	// read number of channels
	c->chan_config = 0;
	c->channels = get_bits(gb, 16) + 1;

	return 0;
}

int ff_mpeg4audio_get_config_gb(MPEG4AudioConfig *c, GetBitContext *gb,
                                int sync_extension)
{
	int specific_config_bitindex, ret;
	int start_bit_index = get_bits_count(gb);
	c->object_type = get_object_type(gb);
	c->sample_rate = get_sample_rate(gb, &c->sampling_index);
	c->chan_config = get_bits(gb, 4);
	if (c->chan_config < FF_ARRAY_ELEMS(ff_mpeg4audio_channels))
	{
		c->channels = ff_mpeg4audio_channels[c->chan_config];
	}
	else
	{
		printf("Invalid chan_config %d\n", c->chan_config);
		return -1;
	}
	c->sbr = -1;
	c->ps  = -1;
	if (c->object_type == AOT_SBR || (c->object_type == AOT_PS &&
	                                  // check for W6132 Annex YYYY draft MP3onMP4
	                                  !(show_bits(gb, 3) & 0x03 && !(show_bits(gb, 9) & 0x3F))))
	{
		if (c->object_type == AOT_PS)
		{
			c->ps = 1;
		}
		c->ext_object_type = AOT_SBR;
		c->sbr = 1;
		c->ext_sample_rate = get_sample_rate(gb, &c->ext_sampling_index);
		c->object_type = get_object_type(gb);
		if (c->object_type == AOT_ER_BSAC)
		{
			c->ext_chan_config = get_bits(gb, 4);
		}
	}
	else
	{
		c->ext_object_type = AOT_NULL;
		c->ext_sample_rate = 0;
	}
	specific_config_bitindex = get_bits_count(gb);

	if (c->object_type == AOT_ALS)
	{
		skip_bits(gb, 5);
		if (show_bits(gb, 24) != MKBETAG('\0', 'A', 'L', 'S'))
		{
			skip_bits(gb, 24);
		}

		specific_config_bitindex = get_bits_count(gb);

		ret = parse_config_ALS(gb, c);
		if (ret < 0)
		{
			return ret;
		}
	}

	if (c->ext_object_type != AOT_SBR && sync_extension)
	{
		while (get_bits_left(gb) > 15)
		{
			if (show_bits(gb, 11) == 0x2b7)   // sync extension
			{
				get_bits(gb, 11);
				c->ext_object_type = get_object_type(gb);
				if (c->ext_object_type == AOT_SBR && (c->sbr = get_bits1(gb)) == 1)
				{
					c->ext_sample_rate = get_sample_rate(gb, &c->ext_sampling_index);
					if (c->ext_sample_rate == c->sample_rate)
					{
						c->sbr = -1;
					}
				}
				if (get_bits_left(gb) > 11 && get_bits(gb, 11) == 0x548)
				{
					c->ps = get_bits1(gb);
				}
				break;
			}
			else
			{
				get_bits1(gb);    // skip 1 bit
			}
		}
	}

	//PS requires SBR
	if (!c->sbr)
	{
		c->ps = 0;
	}
	//Limit implicit PS to the HE-AACv2 Profile
	if ((c->ps == -1 && c->object_type != AOT_AAC_LC) || c->channels & ~0x01)
	{
		c->ps = 0;
	}

	return specific_config_bitindex - start_bit_index;
}

static int decode_eld_specific_config(GetBitContext *gb,
                                      MPEG4AudioConfig *m4ac,
                                      int channel_config)
{
	int ret, ep_config, res_flags;
	uint8_t layout_map[MAX_ELEM_ID * 4][3];
	int tags = 0;
	const int ELDEXT_TERM = 0;

	m4ac->ps  = 0;
	m4ac->sbr = 0;
	m4ac->frame_length_short = get_bits1(gb);

	res_flags = get_bits(gb, 3);
	if (res_flags)
	{
		printf("AAC data resilience (flags %x)\n", res_flags);
		return -1;
	}

	if (get_bits1(gb))   // ldSbrPresentFlag
	{
		printf("Low Delay SBR\n");
		return -1;
	}

	while (get_bits(gb, 4) != ELDEXT_TERM)
	{
		int len = get_bits(gb, 4);
		if (len == 15)
		{
			len += get_bits(gb, 8);
		}
		if (len == 15 + 255)
		{
			len += get_bits(gb, 16);
		}
		if (get_bits_left(gb) < len * 8 + 4)
		{
			printf("overread_err\n");
			return -1;
		}
		skip_bits_long(gb, 8 * len);
	}

	if ((ret = set_default_channel_config(layout_map, &tags, channel_config)))
	{
		return ret;
	}
	/* todo
	if (ac && (ret = output_configure(ac, layout_map, tags, OC_GLOBAL_HDR, 0)))
	    return ret;
	 */

	ep_config = get_bits(gb, 2);
	if (ep_config)
	{
		printf("epConfig %d", ep_config);
		return -1;
	}
	return 0;
}

/**
 * Decode audio specific configuration; reference: table 1.13.
 *
 * @param   ac          pointer to AACContext, may be null
 * @param   avctx       pointer to AVCCodecContext, used for logging
 * @param   m4ac        pointer to MPEG4AudioConfig, used for parsing
 * @param   gb          buffer holding an audio specific config
 * @param   get_bit_alignment relative alignment for byte align operations
 * @param   sync_extension look for an appended sync extension
 *
 * @return  Returns error status or number of consumed bits. <0 - error
 */
static int decode_audio_specific_config_gb(MPEG4AudioConfig *m4ac,
                                           GetBitContext *gb,
                                           int get_bit_alignment,
                                           int sync_extension)
{
	int i, ret;
	GetBitContext gbc = *gb;
	MPEG4AudioConfig m4ac_bak = *m4ac;

	if ((i = ff_mpeg4audio_get_config_gb(m4ac, &gbc, sync_extension)) < 0)
	{
		*m4ac = m4ac_bak;
		return -1;
	}

	if (m4ac->sampling_index > 12)
	{
		printf("invalid sampling rate index %d\n", m4ac->sampling_index);
		*m4ac = m4ac_bak;
		return -1;
	}
	if (m4ac->object_type == AOT_ER_AAC_LD &&
	        (m4ac->sampling_index < 3 || m4ac->sampling_index > 7))
	{
		printf("invalid low delay sampling rate index %d\n", m4ac->sampling_index);
		*m4ac = m4ac_bak;
		return -1;
	}

	skip_bits_long(gb, i);

	switch (m4ac->object_type)
	{
	case AOT_AAC_MAIN:
	case AOT_AAC_LC:
	case AOT_AAC_SSR:
	case AOT_AAC_LTP:
	case AOT_ER_AAC_LC:
	case AOT_ER_AAC_LD:
		if ((ret = decode_ga_specific_config(gb, get_bit_alignment, m4ac, m4ac->chan_config)) < 0)
		{
			return ret;
		}
		break;
	case AOT_ER_AAC_ELD:
		if ((ret = decode_eld_specific_config(gb, m4ac, m4ac->chan_config)) < 0)
		{
			return ret;
		}
		break;
	default:
		printf("Audio object type %s%d\n", m4ac->sbr == 1 ? "SBR+" : "", m4ac->object_type);
		return -1;
	}

	/*printf("AOT %d chan config %d sampling index %d (%d) SBR %d PS %d\n",
	        m4ac->object_type, m4ac->chan_config, m4ac->sampling_index,
	        m4ac->sample_rate, m4ac->sbr,
	        m4ac->ps);*/

	return get_bits_count(gb);
}

static int latm_decode_audio_specific_config(struct LATMContext *latmctx,
                                             GetBitContext *gb, int asclen)
{
	MPEG4AudioConfig m4ac;
	GetBitContext gbc;
	int config_start_bit  = get_bits_count(gb);
	int sync_extension    = 0;
	int bits_consumed, esize, i;

	if (asclen > 0)
	{
		sync_extension = 1;
		asclen         = FFMIN(asclen, get_bits_left(gb));
		init_get_bits(&gbc, gb->buffer, config_start_bit + asclen);
		skip_bits_long(&gbc, config_start_bit);
	}
	else if (asclen == 0)
	{
		gbc = *gb;
	}
	else
	{
		return -1;
	}

	if (get_bits_left(gb) <= 0)
	{
		return -1;
	}

	bits_consumed = decode_audio_specific_config_gb(&m4ac, &gbc, config_start_bit, sync_extension);

	if (bits_consumed < config_start_bit)
	{
		return -1;
	}
	bits_consumed -= config_start_bit;

	if (asclen == 0)
	{
		asclen = bits_consumed;
	}

	if (!latmctx->initialized ||
	        latmctx->m4ac->sample_rate != m4ac.sample_rate ||
	        latmctx->m4ac->chan_config != m4ac.chan_config)
	{

		latmctx->m4ac->sample_rate = m4ac.sample_rate;
		latmctx->m4ac->chan_config = m4ac.chan_config;

		if (latmctx->initialized)
		{
			printf("audio config changed (sample_rate=%d, chan_config=%d)\n", m4ac.sample_rate, m4ac.chan_config);
		}
		else
		{
			printf("initializing latmctx\n");
		}
		latmctx->initialized = 0;

		esize = (asclen + 7) / 8;

		if (latmctx->extradata_size < esize)
		{
			printf("extradata buf to small [%d/%d]\n", latmctx->extradata_size, esize);
		}

		latmctx->extradata_size = esize;
		gbc = *gb;
		for (i = 0; i < esize; i++)
		{
			latmctx->extradata[i] = get_bits(&gbc, 8);
		}
		memset(latmctx->extradata + esize, 0, AV_INPUT_BUFFER_PADDING_SIZE);
	}
	skip_bits_long(gb, asclen);

	return 0;
}

static int read_stream_mux_config(struct LATMContext *latmctx,
                                  GetBitContext *gb)
{
	int ret, audio_mux_version = get_bits(gb, 1);
	int sub_frame_cnt;

	latmctx->audio_mux_version_A = 0;
	if (audio_mux_version)
	{
		latmctx->audio_mux_version_A = get_bits(gb, 1);
	}

	if (0 == latmctx->audio_mux_version_A)
	{

		if (audio_mux_version)
		{
			latm_get_value(gb);    // taraFullness
		}

		latmctx->all_stream_same_time_framing = get_bits(gb, 1);	// allStreamSameTimeFraming
		latmctx->num_sub_frames = get_bits(gb, 6);					// numSubFrames
		latmctx->num_programs = get_bits(gb, 4);					// numPrograms
		if (latmctx->num_programs)
		{
			printf("Multiple programs\n");
			return -1;
		}

		// for each program (which there is only one in DVB)

		// for each layer (which there is only one in DVB)
		if (get_bits(gb, 3))                     // numLayer
		{
			printf("Multiple layers");
			return -1;
		}

		// for all but first stream: use_same_config = get_bits(gb, 1);
		if (!audio_mux_version)
		{
			if ((ret = latm_decode_audio_specific_config(latmctx, gb, 0)) < 0)
			{
				return ret;
			}
		}
		else
		{
			int ascLen = latm_get_value(gb);
			if ((ret = latm_decode_audio_specific_config(latmctx, gb, ascLen)) < 0)
			{
				return ret;
			}
		}

		latmctx->frame_length_type = get_bits(gb, 3);
		switch (latmctx->frame_length_type)
		{
		case 0:
			skip_bits(gb, 8);       // latmBufferFullness
			break;
		case 1:
			latmctx->frame_length = get_bits(gb, 9);
			break;
		case 3:
		case 4:
		case 5:
			skip_bits(gb, 6);       // CELP frame length table index
			break;
		case 6:
		case 7:
			skip_bits(gb, 1);       // HVXC frame length table index
			break;
		}

		latmctx->other_data_present = get_bits(gb, 1);		// other data
		if (latmctx->other_data_present)
		{
			if (audio_mux_version)
			{
				latmctx->other_data_len_bits = latm_get_value(gb);             // other_data_bits
			}
			else
			{
				latmctx->other_data_len_bits = 0;
				int esc, tmp;
				do
				{
					if (get_bits_left(gb) < 9)
					{
						return -1;
					}
					esc = get_bits(gb, 1);
					tmp = get_bits(gb, 8);
					latmctx->other_data_len_bits += tmp;
				}
				while (esc);
			}
		}

		if (get_bits(gb, 1))                     // crc present
		{
			skip_bits(gb, 8);    // config_crc
		}
	}

	return 0;
}

static int read_payload_length_info(struct LATMContext *ctx, GetBitContext *gb)
{
	uint8_t tmp;

	if (ctx->frame_length_type == 0)
	{
		int mux_slot_length = 0;
		do
		{
			if (get_bits_left(gb) < 8)
			{
				return -1;
			}
			tmp = get_bits(gb, 8);
			mux_slot_length += tmp;
		}
		while (tmp == 255);
		return mux_slot_length;
	}
	else if (ctx->frame_length_type == 1)
	{
		return ctx->frame_length;
	}
	else if (ctx->frame_length_type == 3 ||
	         ctx->frame_length_type == 5 ||
	         ctx->frame_length_type == 7)
	{
		skip_bits(gb, 2);          // mux_slot_length_coded
	}
	return 0;
}

static int read_audio_mux_element(struct LATMContext *latmctx, GetBitContext *gb, unsigned char *adts_buf, int *adts_size)
{
	int err, i, j, k, total_size = 0;
	uint8_t use_same_mux;

	use_same_mux = get_bits(gb, 1);
	if (!use_same_mux)
	{
		if ((err = read_stream_mux_config(latmctx, gb)) < 0)
		{
			return err;
		}
	}
	else if (!latmctx->extradata)
	{
		printf("no decoder config found\n");
		return -1;
	}

	if (latmctx->audio_mux_version_A == 0)
	{
		//printf("num_sub_frames:%d\n", latmctx->num_sub_frames);
		for (i = 0; i <= latmctx->num_sub_frames; i++)
		{
			int mux_slot_length_bytes = read_payload_length_info(latmctx, gb);
			//printf("mux_slot_length_bytes:%d\n", mux_slot_length_bytes);
			if (mux_slot_length_bytes < 0 || mux_slot_length_bytes * 8LL > get_bits_left(gb))
			{
				printf("incomplete frame\n");
				return -1;
			}

			if (mux_slot_length_bytes > (*adts_size - total_size))
			{
				printf("need more space [%d/%d]\n", total_size, *adts_size);
				return -1;
			}

			for (j = 0; j < mux_slot_length_bytes; ++j)
			{
				*adts_buf++ = get_bits(gb, 8);
			}

			total_size += mux_slot_length_bytes;
		}

		if (latmctx->other_data_present)
		{
			int other_data_len_bits = latmctx->other_data_len_bits;
			while (other_data_len_bits)
			{
				if (other_data_len_bits >= 8)
				{
					*adts_buf++ = get_bits(gb, 8);
				}
				else
				{
					*adts_buf++ = get_bits(gb, other_data_len_bits);
				}
				total_size++;
				other_data_len_bits -= 8;
			}
		}
		*adts_size = total_size;
		//printf("total_size:%d\n", total_size);
	}

	return 0;
}

static int decode_audio_specific_config(MPEG4AudioConfig *m4ac,
                                        uint8_t *data, int64_t bit_size,
                                        int sync_extension)
{
	int i, ret;
	GetBitContext gb;

	if (bit_size < 0 || bit_size > INT_MAX)
	{
		printf("Audio specific config size is invalid size:%ld\n", bit_size);
		return -1;
	}

	printf("audio specific config size %d\n", (int)bit_size >> 3);
	//for (i = 0; i < bit_size >> 3; i++)
	//    ff_dlog(avctx, "%02x ", data[i]);
	//ff_dlog(avctx, "\n");

	if ((ret = init_get_bits(&gb, data, bit_size)) < 0)
	{
		return ret;
	}

	return decode_audio_specific_config_gb(m4ac, &gb, 0, sync_extension);
}

static int latm_decode_frame(latm_parse_t *latm_parse, unsigned char *data, int size, unsigned char *obuf, int *osize)
{
	int muxlength, err;
	GetBitContext gb;
	struct LATMContext *latmctx;

	if ((NULL == latm_parse) || (NULL == data) || (size <= 0))
	{
		printf("invalid parameter! [0x%p/0x%p/%d]\n", latm_parse, data, size);
		return -1;
	}

	latmctx = &latm_parse->latmctx;

	if ((err = init_get_bits8(&gb, data, size)) < 0)
	{
		printf("<%s:%d>\n", __func__, __LINE__);
		return err;
	}

	// check for LOAS sync word
	if (get_bits(&gb, 11) != LOAS_SYNC_WORD)
	{
		printf("check sync word error!\n");
		return -1;
	}

	muxlength = get_bits(&gb, 13) + 3;
	// not enough data, the parser should have sorted this out
	if (muxlength > size)
	{
		printf("need more data!\n");
		return -1;
	}

	if ((err = read_audio_mux_element(latmctx, &gb, obuf, osize)))
	{
		return (err < 0) ? err : size;
	}

	if (!latmctx->initialized)
	{
		if (!latmctx->extradata)
		{
			//*got_frame_ptr = 0;
			printf("<ah><%s:%d> return\n", __func__, __LINE__);
			return size;
		}
		else
		{
			printf("<ah><%s:%d>\n", __func__, __LINE__);
			//push_output_configuration(&latmctx->aac_ctx);
			if ((err = decode_audio_specific_config(
			               latmctx->m4ac, latmctx->extradata, latmctx->extradata_size * 8LL, 1)) < 0)
			{
				//pop_output_configuration(&latmctx->aac_ctx);
				return err;
			}
			latmctx->initialized = 1;
		}
	}

	if (show_bits(&gb, 12) == 0xfff)
	{
		printf("<ah>parse latm error!\n");
		return -1;
	}

	//*obuf = (data + gb.index / 8);
	//*osize = size - (gb.index / 8);

	//printf("%d %d[%02x/%02x/%02x/%02x] %d %d\n", gb.index, get_bits_left(&gb), 
	//	(*obuf)[0], (*obuf)[1], (*obuf)[2], (*obuf)[3], *osize, latmctx->m4ac->object_type);


	return muxlength;
}

static int adts_write_frame_header(MPEG4AudioConfig *m4ac, uint8_t *header, int pce_size)
{
	PutBitContext pb;

	unsigned full_frame_size = (unsigned)ADTS_HEADER_SIZE + pce_size;
	/*if (full_frame_size > ADTS_MAX_FRAME_BYTES)
	{
		printf("ADTS frame size too large: %u (max %d)\n",
		       full_frame_size, ADTS_MAX_FRAME_BYTES);
		return -1;
	}*/
	
	init_put_bits(&pb, header, ADTS_HEADER_SIZE);

	int objecttype = m4ac->object_type;;
	int sample_rate_index = m4ac->sampling_index;
	int channel_conf = m4ac->chan_config;
	//printf("%d %d %d\n", objecttype, sample_rate_index, channel_conf);

	/* adts_fixed_header */
	put_bits(&pb, 12, 0xfff);   /* syncword */
	put_bits(&pb, 1, 0);        /* ID */
	put_bits(&pb, 2, 0);        /* layer */
	put_bits(&pb, 1, 1);        /* protection_absent */
	put_bits(&pb, 2, objecttype); /* profile_objecttype */
	put_bits(&pb, 4, sample_rate_index);
	put_bits(&pb, 1, 0);        /* private_bit */
	put_bits(&pb, 3, channel_conf); /* channel_configuration */
	put_bits(&pb, 1, 0);        /* original_copy */
	put_bits(&pb, 1, 0);        /* home */

	/* adts_variable_header */
	put_bits(&pb, 1, 0);        /* copyright_identification_bit */
	put_bits(&pb, 1, 0);        /* copyright_identification_start */
	put_bits(&pb, 13, full_frame_size); /* aac_frame_length */
	put_bits(&pb, 11, 0x7ff);   /* adts_buffer_fullness */
	put_bits(&pb, 2, 0);        /* number_of_raw_data_blocks_in_frame */

	flush_put_bits(&pb);

	return 0;
}

int latm_find_header(const unsigned char *data, int size, int *offset)
{
	int of = 0, ret = -1;

	if ((NULL == data) || (size <= 0) || (NULL == offset))
	{
		printf("<%s:%d> Invalid parameter [0x%p/%d/0x%p]\n", __func__, __LINE__, data, size, offset);
		return -1;
	}

	while (of < (size - 2))
	{
		if ((0x56 == (data[of] & 0xff)) && (0xe0 == (data[of + 1] & 0xf0)))
		{
			ret = 0;
			break;
		}
		of++;
	}

	*offset = of;

	return ret;
}

int main(int argc, char *argv[])
{
	int fd_input, fd_output;
	unsigned char buf[8192];
	unsigned char adts_header[ADTS_HEADER_SIZE];
	int data_left = 0, ret, size, read_over = 0, offset;
	uint8_t adts_buf[8192];
	int adts_frm_len = 8192, latm_frm_len;
	latm_parse_t latm_parse;

	latm_parse.latmctx.m4ac = &latm_parse.m4ac;
	latm_parse.latmctx.extradata_size = sizeof(latm_parse.latmctx.extradata);

	if (argc < 3)
	{
		printf("usage:./%s input.aac output.wav\n", argv[0]);
		return 0;
	}

	printf("input file:%s\n", argv[1]);
	printf("output file:%s\n", argv[2]);

	fd_input = open(argv[1], O_RDONLY);
	if (fd_input <= 0)
	{
		printf("open %s error!\n", argv[1]);
		return -1;
	}

	fd_output = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
	if (fd_output <= 0)
	{
		printf("open %s error!\n", argv[2]);
		return -1;
	}

	while (1)
	{
		if (0 == read_over && data_left < sizeof(buf))
		{
			size = read(fd_input, buf + data_left, sizeof(buf) - data_left);
			if (0 == size)
			{
				printf("read file over!\n");
				read_over = 1;
				//break;
			}

			if (size < 0)
			{
				printf("read file error!\n");
				break;
			}
			data_left += size;
		}
		//printf("size:%d data_left:%d\n", size, data_left);

		if (data_left < latm_frm_len)
		{
			printf("parse latm over!\n");
			break;
		}
		
		ret = latm_find_header(buf, data_left, &offset);
		data_left -= offset;
		memmove(buf, buf + offset, data_left);
		if (ret < 0)
		{
			printf("find latm header error!\n");
			continue;
		}

		latm_frm_len = ((buf[1] & 0x1f) << 8) + buf[2] + 3;
		if (latm_frm_len > data_left)
		{
			printf("need more data![%d/%d]\n", latm_frm_len, data_left);
			continue;
		}
		//printf("latm_frm_len:%d\n", latm_frm_len);

		//func_latm2adts(buf, latm_frm_len, &adts_buf, &adts_frm_len, &asc);
		//printf("latm_frm_len:%d\n", g_latm_frm_len);
		adts_frm_len = sizeof(adts_buf);
		latm_decode_frame(&latm_parse, buf, latm_frm_len, adts_buf, &adts_frm_len);

		data_left -= latm_frm_len;
		memmove(buf, buf + latm_frm_len, data_left);
		if ((adts_frm_len <= 0))
		{
			printf("parse latm error![%d]\n", adts_frm_len);
			continue;
		}

		adts_write_frame_header(&latm_parse.m4ac, adts_header, adts_frm_len);
		write(fd_output, adts_header, ADTS_HEADER_SIZE);
		write(fd_output, adts_buf, adts_frm_len);
	}

	close(fd_input);
	close(fd_output);

	return 0;
}

