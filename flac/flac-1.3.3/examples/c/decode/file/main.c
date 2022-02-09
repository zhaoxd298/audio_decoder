/* example_c_decode_file - Simple FLAC file decoder using libFLAC
 * Copyright (C) 2007-2009  Josh Coalson
 * Copyright (C) 2011-2016  Xiph.Org Foundation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * This example shows how to use libFLAC to decode a FLAC file to a WAVE
 * file.  It only supports 16-bit stereo files.
 *
 * Complete API documentation can be found at:
 *   http://xiph.org/flac/api/
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "share/compat.h"
#include "FLAC/stream_decoder.h"

#define FLAC_SAMPLES_PER_FRAME 	(1152)
#define FLAC_PCM_BUF_SAMPLES	(8192)
#define FLAC_PCM_BUF_SIZE 		(FLAC_PCM_BUF_SAMPLES << 2)

struct flac_lib
{
	FLAC__StreamDecoder *decoder;

	unsigned int* left_pcm_buf;
	unsigned int* right_pcm_buf;
	
	unsigned int total_samples;
	unsigned int sample_num;
	unsigned int sample_rate;
	unsigned int ch_num;
	unsigned int bit_per_sample;

	unsigned int min_blocksize;
	unsigned int max_blocksize;
	unsigned int min_framesize;
	unsigned int max_framesize;
	unsigned int eos;
} g_flac_lib;

static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	uint32_t size;
	struct flac_lib* flac_lib = &g_flac_lib;

	(void)decoder;
	(void)client_data;

	if (buffer [0] == NULL)
	{
		fprintf(stderr, "ERROR: buffer [0] is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}
	
	if (buffer [1] == NULL)
	{
		fprintf(stderr, "ERROR: buffer [1] is NULL\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	size = frame->header.blocksize;
	if (size > (FLAC_PCM_BUF_SAMPLES - flac_lib->sample_num))
	{
		size = FLAC_PCM_BUF_SAMPLES - flac_lib->sample_num;
		printf("<%s:%d> pcm buf is full, discard %d samples!\n", __func__, __LINE__, frame->header.blocksize - size);
	}

	memcpy(flac_lib->left_pcm_buf+flac_lib->sample_num, buffer[0], size << 2);
	memcpy(flac_lib->right_pcm_buf+flac_lib->sample_num, buffer[1], size << 2);
	flac_lib->sample_num += size;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	struct flac_lib* flac_lib = &g_flac_lib;
	(void)decoder, (void)client_data;

	/* print some stats */
	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{
		/* save for later */
		flac_lib->total_samples = metadata->data.stream_info.total_samples;
		flac_lib->sample_rate = metadata->data.stream_info.sample_rate;
		flac_lib->ch_num = metadata->data.stream_info.channels;
		flac_lib->bit_per_sample = metadata->data.stream_info.bits_per_sample;

		flac_lib->min_blocksize = metadata->data.stream_info.min_blocksize;
		flac_lib->max_blocksize = metadata->data.stream_info.max_blocksize;
		flac_lib->min_framesize = metadata->data.stream_info.min_framesize;
		flac_lib->max_framesize = metadata->data.stream_info.max_framesize;
		
		printf("sample rate    : %u\n", flac_lib->sample_rate);
		printf("channels       : %u\n", flac_lib->ch_num);
		printf("bits per sample: %u\n", flac_lib->bit_per_sample);
		printf("total samples  : %d\n", flac_lib->total_samples);
		printf("min_blocksize  : %d\n", flac_lib->min_blocksize);
		printf("max_blocksize  : %d\n", flac_lib->max_blocksize);
		printf("min_framesize  : %d\n", flac_lib->min_framesize);
		printf("max_framesize  : %d\n", flac_lib->max_framesize);
	}
}

static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;

	fprintf(stderr, "Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

static FLAC__bool flac_decoder_init(void)
{
	FLAC__StreamDecoderInitStatus init_status;
	struct flac_lib* flac_lib = &g_flac_lib;

	flac_lib->total_samples = 0;
	flac_lib->sample_num = 0;
	flac_lib->sample_rate = 0;
	flac_lib->ch_num = 0;
	flac_lib->bit_per_sample = 0;
	flac_lib->min_blocksize = 0;
	flac_lib->max_blocksize = 0;
	flac_lib->min_framesize = 0;
	flac_lib->max_framesize = 0;
	flac_lib->eos = 0;

	if ((flac_lib->decoder = FLAC__stream_decoder_new()) == NULL)
	{
		printf("ERROR: allocating decoder\n");
		return false;
	}

	(void)FLAC__stream_decoder_set_md5_checking(flac_lib->decoder, true);

	init_status = FLAC__stream_decoder_init_file(flac_lib->decoder, NULL, write_callback, metadata_callback, error_callback, NULL);
	if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
	{
		printf("ERROR: initializing decoder: %s\n", FLAC__StreamDecoderInitStatusString[init_status]);
		FLAC__stream_decoder_delete(flac_lib->decoder);
		return false;
	}

#if 0
	flac_lib->left_pcm_buf = dl_malloc(FLAC_PCM_BUF_SIZE);	
	if (NULL == flac_lib->left_pcm_buf)
	{
		FATAL_ERROR("<%s:%d> malloc left_pcm_buf error!\n", __func__, __LINE__);
		FLAC__stream_decoder_delete(flac_lib->decoder);
		return false;
	}

	flac_lib->right_pcm_buf = dl_malloc(FLAC_PCM_BUF_SIZE);
	if (NULL == flac_lib->right_pcm_buf)
	{
		FATAL_ERROR("<%s:%d> malloc right_pcm_buf error!\n", __func__, __LINE__);
		FLAC__stream_decoder_delete(flac_lib->decoder);
		dl_free(flac_lib->left_pcm_buf);
		return false;
	}
#else
	flac_lib->left_pcm_buf = malloc(FLAC_PCM_BUF_SIZE);	
	if (NULL == flac_lib->left_pcm_buf)
	{
		printf("<%s:%d> malloc left_pcm_buf error!\n", __func__, __LINE__);
		FLAC__stream_decoder_delete(flac_lib->decoder);
		return false;
	}

	flac_lib->right_pcm_buf = malloc(FLAC_PCM_BUF_SIZE);
	if (NULL == flac_lib->right_pcm_buf)
	{
		printf("<%s:%d> malloc right_pcm_buf error!\n", __func__, __LINE__);
		FLAC__stream_decoder_delete(flac_lib->decoder);
		free(flac_lib->left_pcm_buf);
		return false;
	}
#endif

	return true;
}

static FLAC__bool flac_decoder_deinit(void)
{
	struct flac_lib* flac_lib = &g_flac_lib;

	FLAC__stream_decoder_delete(flac_lib->decoder);

	if (flac_lib->left_pcm_buf)
	{
		free(flac_lib->left_pcm_buf);
		flac_lib->left_pcm_buf = NULL;
	}

	if (flac_lib->right_pcm_buf)
	{
		free(flac_lib->right_pcm_buf);
		flac_lib->right_pcm_buf = NULL;
	}

	return true;
}


static FLAC__bool flac_decode(FLAC__StreamDecoder *decoder, const char *buf, int size, int *used)
{
	FLAC__bool ret = false;

	if ((NULL == decoder) || (NULL == buf) || (size <= 0) || (NULL == used))
	{
		return false;
	}

	FLAC__stream_input_data(decoder, buf, size, used);

	if (FLAC__stream_get_skip_size(decoder))
	{
		// skip data
		FLAC__stream_skip_data(decoder);
		return true;
	}

	ret = FLAC__stream_decode(decoder);

	return ret;
}

int main(int argc, char *argv[])
{
	FLAC__bool ok = true;
	FILE *fout, *fin;
	struct flac_lib* flac_lib = &g_flac_lib;

	FLAC__bool ret = false;
	int rd_size = 0;
	static int used = 0, left_size = 0;
	char buf[16384];	// 16384

	if (argc != 3)
	{
		fprintf(stderr, "usage: %s infile.flac outfile.wav\n", argv[0]);
		return 1;
	}

	if ((fin = fopen(argv[1], "rb")) == NULL)
	{
		fprintf(stderr, "ERROR: opening %s for input\n", argv[1]);
		return 1;
	}
	
	if ((fout = fopen(argv[2], "wb")) == NULL)
	{
		fprintf(stderr, "ERROR: opening %s for output\n", argv[2]);
		return 1;
	}

	ok = flac_decoder_init();
	if (false == ok)
	{
		return false;
	}

	while (1)
	{
		//unsigned int es_data = FLAC__stream_get_es_data_left(flac_lib->decoder);
		//unsigned int max_framesize = decoder->private_->stream_info.data.stream_info.max_framesize;

		if (left_size < sizeof(buf))
		{
			rd_size = fread(buf+left_size, 1, sizeof(buf)-left_size, fin);
			if (rd_size < 0)
			{
				printf("<%s:%d> read error!\n", __func__, __LINE__);
				break;
			}
			else if (0 == rd_size)
			{
				//if (0 == es_data)
				{
					printf("<%s:%d> read file over!\n", __func__, __LINE__);
					break;
				}
			}

			left_size += rd_size;
		}

		if (left_size > 0)
		{
			used = 0;
			ret = flac_decode(flac_lib->decoder, buf, left_size, &used);
			
			memmove(buf, buf+used, left_size-used);
			left_size -= used;
		}

		if (FLAC__STREAM_DECODER_END_OF_STREAM == FLAC__stream_decoder_state(flac_lib->decoder))
		{
			flac_lib->eos = 1;
			break;
		}

		if (flac_lib->sample_num)
		{
			//fwrite(flac_lib->left_pcm_buf, 1, flac_lib->sample_num*4, fout);
			fwrite(flac_lib->right_pcm_buf, 1, flac_lib->sample_num*4, fout);
			flac_lib->sample_num = 0;
		}
	}

	flac_decoder_deinit();
	fclose(fin);
	fclose(fout);

	return 0;
}

