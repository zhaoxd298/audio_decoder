/*

demac - A Monkey's Audio decoder

$Id$

Copyright (C) Dave Chapman 2007

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA

*/

/*

This example is intended to demonstrate how the decoder can be used in
embedded devices - there is no usage of dynamic memory (i.e. no
malloc/free) and small buffer sizes are chosen to minimise both the
memory usage and decoding latency.

This implementation requires the following memory and supports decoding of all APE files up to 24-bit Stereo.

32768 - data from the input stream to be presented to the decoder in one contiguous chunk.
18432 - decoding buffer (left channel)
18432 - decoding buffer (right channel)

17408+5120+2240 - buffers used for filter histories (compression levels 2000-5000)

In addition, this example uses a static 27648 byte buffer as temporary
storage for outputting the data to a WAV file but that could be
avoided by writing the decoded data one sample at a time.

*/

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

//#include "demac.h"
//#include "wavwrite.h"
#include "libdemac/decoder.h"

#define BLOCKS_PER_LOOP     1152

#define INPUT_CHUNKSIZE     (20 * 1024)
#define APE_PCM_BUF_SIZE	(BLOCKS_PER_LOOP << 3)

enum ape_decode_state_e
{
	APE_STATE_PARSE_HEADER = 0,
	APE_STATE_PARSE_SEEK_TABLE,
	APE_STATE_SEEK_TO_FIRST_FRAME,
	APE_STATE_DECODE_FRAME,
};

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

struct ape_lib
{
	struct ape_ctx_t ape_ctx;

	int es_data_size;
	unsigned char *es_buf;

	int *decoded0;
	int *decoded1;

	unsigned int *pcm_left_ch;
	unsigned int *pcm_right_ch;
	unsigned int pcm_samples;

	int first_byte;

	enum ape_decode_state_e state;
	int total_skip_size;
	int seek_table_offset;
	int eos;
} g_ape_lib;

static int ape_decode_init(struct ape_lib *ape_lib)
{
	if (NULL == ape_lib)
	{
		return -1;
	}

	ape_lib->es_buf = malloc(INPUT_CHUNKSIZE);
	if (NULL == ape_lib->es_buf)
	{
		printf("<%s:%d> malloc es buf error!\n", __func__, __LINE__);
		return -1;
	}

	ape_lib->decoded0 = malloc(BLOCKS_PER_LOOP << 2);
	if (NULL == ape_lib->decoded0)
	{
		printf("<%s:%d> malloc decoded0 error!\n", __func__, __LINE__);
		goto alloc_decoded0_err;
	}

	ape_lib->decoded1 = malloc(BLOCKS_PER_LOOP << 2);
	if (NULL == ape_lib->decoded1)
	{
		printf("<%s:%d> malloc decoded1 error!\n", __func__, __LINE__);
		goto alloc_decoded1_err;
	}
	
	ape_lib->pcm_left_ch = malloc(APE_PCM_BUF_SIZE);
	if (NULL == ape_lib->pcm_left_ch)
	{
		printf("<%s:%d> malloc pcm_left_ch error!\n", __func__, __LINE__);
		goto alloc_pcm_left_ch_err;
	}

	ape_lib->pcm_right_ch = malloc(APE_PCM_BUF_SIZE);
	if (NULL == ape_lib->pcm_right_ch)
	{
		printf("<%s:%d> malloc pcm_right_ch error!\n", __func__, __LINE__);
		goto alloc_pcm_right_ch_err;
	}

	ape_lib->pcm_samples = 0;
	ape_lib->es_data_size = 0;
	ape_lib->ape_ctx.cur_frame = 0;

	ape_lib->first_byte = 3;

	ape_lib->state = APE_STATE_PARSE_HEADER;
	ape_lib->total_skip_size = 0;
	ape_lib->seek_table_offset = 0;
	ape_lib->eos = 0;

	return 0;

alloc_pcm_right_ch_err:
	free(ape_lib->pcm_left_ch);
	ape_lib->pcm_left_ch = NULL;
alloc_pcm_left_ch_err:
	free(ape_lib->decoded1);
	ape_lib->decoded1 = NULL;
alloc_decoded1_err:
	free(ape_lib->decoded0);
	ape_lib->decoded0 = NULL;
alloc_decoded0_err:
	free(ape_lib->es_buf);
	ape_lib->es_buf = NULL;
	
	return -1;
}

static int ape_decode_release(struct ape_lib *ape_lib)
{
	if (NULL == ape_lib)
	{
		return -1;
	}

	if (ape_lib->es_buf)
	{
		free(ape_lib->es_buf);
		ape_lib->es_buf = NULL;
	}

	if (ape_lib->decoded0)
	{
		free(ape_lib->decoded0);
		ape_lib->decoded0 = NULL;
	}

	if (ape_lib->decoded1)
	{
		free(ape_lib->decoded1);
		ape_lib->decoded1 = NULL;
	}

	if (ape_lib->pcm_left_ch)
	{
		free(ape_lib->pcm_left_ch);
		ape_lib->pcm_left_ch = NULL;
	}

	if (ape_lib->pcm_right_ch)
	{
		free(ape_lib->pcm_right_ch);
		ape_lib->pcm_left_ch = NULL;
	}

	if (ape_lib->ape_ctx.frames)
	{
		free(ape_lib->ape_ctx.frames);
		ape_lib->ape_ctx.frames = NULL;
	}
	
	return 0;
}

int ape_decode(struct ape_lib *ape_lib, unsigned char *buf, int *size)
{
	static int nblocks, frm_size;
	int used, i, ret;
	struct ape_ctx_t *ape_ctx = NULL;

	if ((NULL == ape_lib) || (NULL == buf) || (NULL == size))
	{
		printf("<%s:%d> Invalid parameter!\n", __func__, __LINE__);
		return -1;
	}
	
	ape_ctx = &ape_lib->ape_ctx;
	
	/*if (ape_ctx->cur_frame < (ape_ctx->totalframes - 2))
	{
		if ((*size) < INPUT_CHUNKSIZE/2)
		{
			return -1;
		}
	}*/

	if (ape_ctx->new_frame)
	{
		// insure enough data for decode
		/*if (ape_ctx->frames)
		{
			if (*size < ape_ctx->frames[ape_ctx->cur_frame].size)
			{
				//printf("id:%d frm_size:%d %d\n", ape_ctx->cur_frame, ape_ctx->frames[ape_ctx->cur_frame].size, *size);
				return -1;
			}
		}*/

		frm_size = ape_ctx->frames[ape_ctx->cur_frame].size;
		
		/* Calculate how many blocks there are in this frame */
		if (ape_ctx->cur_frame == (ape_ctx->totalframes - 1))
		{
			nblocks = ape_ctx->finalframeblocks;
		}
		else
		{
			nblocks = ape_ctx->blocksperframe;
		}
		if (0 == ape_ctx->cur_frame%10)
			printf("id:%d block:%d size:%d\n", ape_ctx->cur_frame, nblocks, frm_size);
		ape_ctx->currentframeblocks = nblocks;

		used = 0;
		/* Initialise the frame decoder */
		init_frame_decoder(ape_ctx, buf, &ape_lib->first_byte, &used);
		//printf("frm:%d used0:%d firstbyte:%d\n", currentframe, bytesconsumed, firstbyte);

		if (*size < used)
		{
			printf("L%d critical error! size:%d used:%d\n",__LINE__, *size, used);
			return -1;
		}
		
		/* Update buffer */
		memmove(buf, buf + used, *size - used);
		*size -= used;
		frm_size -= used;
		
		ape_ctx->new_frame = 0;
	}

	if (ape_ctx->cur_frame < (ape_ctx->totalframes - 1))
	{
		if (frm_size < 0)
		{
			printf("frm_size:%d\n", frm_size);
		}

		if (*size < MIN(frm_size, 10240))
		{
			return -1;
		}
	}
	/* Decode the frame a chunk at a time */
	int sub_blocks = MIN(BLOCKS_PER_LOOP, nblocks);
	ret = decode_chunk(ape_ctx, buf, &ape_lib->first_byte, &used, ape_lib->decoded0, ape_lib->decoded1, sub_blocks);
	if (ret < 0)
	{
		/* Frame decoding error, abort */
		printf("decode err, used:%d\n", used);
		return ret;
	}

	//printf("L%d size:%d used:%d\n", __LINE__, *size, used);

	if (*size < used)
	{
		printf("L%d critical error! size:%d used:%d sub_block:%d block:%d\n",__LINE__, *size, used, sub_blocks, nblocks);
		exit(1);
		return -1;
	}

	/* Update the buffer */
	memmove(buf, buf + used, *size - used);
	*size -= used;
	frm_size -= used;

	/* Convert the output samples to WAV format and write to output file */
	if (ape_ctx->bps == 8)
	{
		for (i = 0; i < sub_blocks; i++)
		{
			/* 8 bit WAV uses unsigned samples */
			ape_lib->pcm_left_ch[ape_lib->pcm_samples] = (ape_lib->decoded0[i] + 0x80) & 0xff;

			if (ape_ctx->channels == 2)
			{
				ape_lib->pcm_right_ch[ape_lib->pcm_samples] = (ape_lib->decoded1[i] + 0x80) & 0xff;
			}
			ape_lib->pcm_samples++;
		}
	}
	else
	{
		for (i = 0 ; i < sub_blocks ; i++)
		{
			ape_lib->pcm_left_ch[ape_lib->pcm_samples] = ape_lib->decoded0[i];

			if (ape_ctx->channels == 2)
			{
				ape_lib->pcm_right_ch[ape_lib->pcm_samples] = ape_lib->decoded1[i];;
			}
			ape_lib->pcm_samples++;
		}
	}

	/* Decrement the block count */
	nblocks -= sub_blocks;
	if (nblocks <= 0)
	{
		ape_ctx->new_frame = 1;
		ape_ctx->cur_frame++;
	}

	return 0;
}

/*return consumed size*/
static int ape_input_data(struct ape_lib *ape_lib, char *buf, int size)
{
	if ((NULL == ape_lib) || (NULL == buf) || (size <= 0))
	{
		return -1;
	}

	if (size > INPUT_CHUNKSIZE - ape_lib->es_data_size)
	{
		size = INPUT_CHUNKSIZE - ape_lib->es_data_size;
	}

	if (size > 0)
	{
		memcpy(ape_lib->es_buf + ape_lib->es_data_size, buf, size);
		ape_lib->es_data_size += size;
	}
	
	return size;
}

int main(int argc, char* argv[])
{
	int fin, fout, ret, read_over = 0;
	struct ape_lib *ape_lib = &g_ape_lib;

	int data_size = 0;
	char buf[2048];
	
	if (argc != 3)
	{
		fprintf(stderr, "Usage: demac infile.ape outfile.wav\n");
		return 0;
	}

	fin = open(argv[1], O_RDONLY);
	if (fin < 0)
	{
		return -1;
	}

	fout = open(argv[2], O_CREAT|O_WRONLY|O_TRUNC, 0644);

	ape_decode_init(ape_lib);

	while (1)
	{
		if (0 == read_over)
		{
			ret = sizeof(buf) - data_size;
			if (ret > 0)
			{
				ret = read(fin, buf + data_size, ret);
				if (ret < 0)
				{
					printf("read error!\n");
					return -1;
				}
				else if (0 == ret)
				{
					printf("read over!\n");
					read_over = 1;
				}
				//printf("read:%d/%ld\n", ret, sizeof(buf) - data_size);
				data_size += ret;
			}
		}

		if (data_size > 0)
		{
			ret = ape_input_data(ape_lib, buf, data_size);
			memmove(buf, buf+ret, data_size-ret);
			data_size -= ret;
		}
		
		if (APE_STATE_PARSE_HEADER == ape_lib->state)	//parse header
		{
			/*if (ape_lib->es_data_size < 11420)	// ????
			{
				continue;
			}*/
			ret = ape_parseheaderbuf(&ape_lib->ape_ctx, ape_lib->es_buf, ape_lib->es_data_size);
			if (ret >= 0)
			{
				ape_lib->seek_table_offset = ret;
				
				ape_lib->state = APE_STATE_PARSE_SEEK_TABLE;
			}
		}

		if (APE_STATE_PARSE_SEEK_TABLE == ape_lib->state)	//parse seek table
		{
			ret = ape_parse_seek_table(&ape_lib->ape_ctx, ape_lib->es_buf, ape_lib->es_data_size, ape_lib->seek_table_offset);
			if (0 == ret)
			{
				//ape_dumpinfo(&ape_lib->ape_ctx);
				
				ape_lib->state = APE_STATE_SEEK_TO_FIRST_FRAME;
				ape_lib->ape_ctx.new_frame = 1;
			}
		}

		if (APE_STATE_SEEK_TO_FIRST_FRAME == ape_lib->state)	// seek to first frame
		{
			if (ape_lib->ape_ctx.firstframe > ape_lib->total_skip_size)
			{
				int skip = ape_lib->ape_ctx.firstframe - ape_lib->total_skip_size;
				if (skip > ape_lib->es_data_size)
				{
					ape_lib->total_skip_size += ape_lib->es_data_size;
					ape_lib->es_data_size = 0;
				}
				else
				{
					ape_lib->total_skip_size += skip;
					memmove(ape_lib->es_buf, ape_lib->es_buf+skip, ape_lib->es_data_size - skip);
					ape_lib->es_data_size = ape_lib->es_data_size-skip;
				}
				continue;
			}
			else
			{
				ape_lib->state = APE_STATE_DECODE_FRAME;
			}
		}

		if (APE_STATE_DECODE_FRAME == ape_lib->state)	// decode
		{
			ape_decode(ape_lib, ape_lib->es_buf, &ape_lib->es_data_size);
		}

		if (ape_lib->pcm_samples > 0)
		{
			ret = write(fout, ape_lib->pcm_right_ch, ape_lib->pcm_samples * 4);
			ape_lib->pcm_samples = 0;
		}

		// check decode over
		if (ape_lib->ape_ctx.cur_frame == ape_lib->ape_ctx.totalframes)
		{
			printf("decode success, %d samples!\n", ape_lib->ape_ctx.totalframes);
			ape_lib->eos = 1;
			break;
		}
	}
	
	ape_decode_release(ape_lib);

	close(fin);
	close(fout);
	
	return 0;
}
