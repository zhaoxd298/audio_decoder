/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**
** $Id: main.c,v 1.89 2015/01/19 09:46:12 knik Exp $
**/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#ifndef __MINGW32__
#define off_t __int64
#endif
#else
#include <time.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include <neaacdec.h>
#include <sys/types.h>

#include "get_bits.h"

#include "unicode_support.h"
#include "audio.h"
#include "../libfaad/common.h"
#include "../libfaad/structs.h"

/* MicroSoft channel definitions */
#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#define SPEAKER_RESERVED               0x80000000

#define PCM_BUF_SIZE (16 * 1024)


typedef struct aac_lib_s
{
	NeAACDecStruct decoder;
	NeAACDecFrameInfo frame_info;
	NeAACDecConfiguration *config;
	char pcm_buf[PCM_BUF_SIZE];
	//real_t time_out[MAX_CHANNELS][AAC_FRM_LEN * 2];
} aac_lib_t;

static long aacChannelConfig2wavexChannelMask(NeAACDecFrameInfo *hInfo)
{
	if (hInfo->channels == 6 && hInfo->num_lfe_channels)
	{
		return SPEAKER_FRONT_LEFT + SPEAKER_FRONT_RIGHT +
		       SPEAKER_FRONT_CENTER + SPEAKER_LOW_FREQUENCY +
		       SPEAKER_BACK_LEFT + SPEAKER_BACK_RIGHT;
	}
	else
	{
		return 0;
	}
}

#define AAC_ADTS_HEADER_SIZE 7

const int adts_sample_rates[16] = {
	96000, 88200, 64000, 48000, 44100, 32000,
	24000, 22050, 16000, 12000, 11025, 8000, 7350
};
	
#if 0
static int adts_parse(const unsigned char *buf, int size)
{
	int frames, frame_length, bitrate, length;
	int t_framelength = 0;
	int samplerate;
	float frames_per_sec, bytes_per_frame;

	/* Read all frames to ensure correct time and bitrate */
	if (size > 7)
	{
		/* check syncword */
		if (!((buf[0] == 0xFF) && ((buf[1] & 0xF6) == 0xF0)))
		{
			return -1;
		}

		samplerate = adts_sample_rates[(buf[2] & 0x3c) >> 2];

		frame_length = ((((unsigned int)buf[3] & 0x3)) << 11)
		               | (((unsigned int)buf[4]) << 3) | (buf[5] >> 5);
		if (frame_length == 0)
		{
			return -1;
		}

		t_framelength += frame_length;
	}

	frames_per_sec = (float)samplerate / 1024.0f;
	if (frames != 0)
	{
		bytes_per_frame = (float)t_framelength / (float)(frames * 1000);
	}
	else
	{
		bytes_per_frame = 0;
	}
	bitrate = (int)(8. * bytes_per_frame * frames_per_sec + 0.5);
	if (frames_per_sec != 0)
	{
		length = (float)frames / frames_per_sec;
	}
	else
	{
		length = 1;
	}

	printf("frame_length:%d\n", frame_length);
	printf("frames_per_sec:%f\n", frames_per_sec);
	printf("samplerate:%d\n", samplerate);
	printf("bitrate:%d\n", bitrate);
	printf("length:%d\n", length);

	return 1;
}
#endif


static int find_adts_header(const unsigned char *buf, int size)
{
	int offset = 0;
	
	if ((NULL == buf) || (size <= 0))
	{
		printf("<%s:%d> Invalid parameter!\n", __func__, __LINE__);
		return -1;
	}

	while (size >= 2)
	{
		if ((buf[0] == 0xFF) && ((buf[1] & 0xF0) == 0xF0))
		{
			break;
		}
		offset++;
		buf++;
		size--;
	}

	return offset;
}

int g_frm_len, g_sample_rate;
int aac_adts_parse(const unsigned char *buf, int size)
{
	int rdb, ch, sr_idx, frm_len, sample_rate, samples;
	int aot, crc_abs;
	int offset = 0;
	GetBitContext gbc = {0};
	
	if ((NULL == buf) || (size <= 0))
	{
		printf("<%s:%d> Invalid parameter!\n", __func__, __LINE__);
		return -1;
	}

	offset = find_adts_header(buf, size);
	if ((offset < 0) || (size - offset <= 2))
	{
		printf("find adts header error, offset:%d\n", offset);
		// size = 0;
		return -1;
	}

	buf += offset;
	size -= offset;

	if (size < AAC_ADTS_HEADER_SIZE)
	{
		printf("need more data!\n");
		return -1;
	}

	gbc.buffer = buf;
	gbc.buffer_end = buf + size;
	if (get_bits(&gbc, 12) != 0xfff)
	{
		printf("adts header error!\n");
		return -1;
	}

	skip_bits1(&gbc);				/* id */
	skip_bits(&gbc, 2);				/* layer always 0*/
	crc_abs = get_bits1(&gbc);		/* protection_absent */
	aot = get_bits(&gbc, 2);		/* profile_objecttype */
	sr_idx = get_bits(&gbc, 4);		/* sample_frequency_index */
	sample_rate = adts_sample_rates[sr_idx];
	skip_bits1(&gbc);				/* private_bit */
	ch = get_bits(&gbc, 3);			/* channel_configuration */

	skip_bits1(&gbc);				/* original/copy */
	skip_bits1(&gbc);				/* home */

	/* adts_variable_header */
	skip_bits1(&gbc);				/* copyright_identification_bit */
	skip_bits1(&gbc);				/* copyright_identification_start */
	frm_len = get_bits(&gbc, 13);	/* aac_frame_length */
	
	skip_bits(&gbc, 11); 		 /* adts_buffer_fullness */
	rdb = get_bits(&gbc, 2); 	 /* number_of_raw_data_blocks_in_frame */

	samples = (rdb + 1) * 1024;
	/*printf("\nobject_type:%d\n", aot + 1);
	printf("chan_config:%d\n", ch);
	printf("crc_absent:%d\n", crc_abs);
	printf("num_aac_frames:%d\n", rdb + 1);
	printf("sampling_index:%d\n", sr_idx);
	printf("sample_rate:%d\n", sample_rate);
	printf("samples:%d\n", samples);
	printf("bit_rate:%d\n", frm_len * 8 * sample_rate / samples);
	printf("frm_len:%d\n\n", frm_len);*/

	g_frm_len = frm_len;
	g_sample_rate = sample_rate;
	
	return offset;
}

static int aud_aac_init(void *handle, int size, unsigned long param)
{
	int i;
	aac_lib_t *aac_lib = NULL;

	if ((NULL == handle) || (size <= 0))
	{
		printf("<%s:%d> invalid parameter! [0x%p/%d]\n", __func__, __LINE__, handle, size);
		return -1;
	}

	if (size < sizeof(aac_lib_t))
	{
		printf("<%s:%d> decoder buf to small [%d/%ld]\n", __func__, __LINE__, size, sizeof(aac_lib_t));
		return -1;
	}
	printf("<ah><%s:%d>\n", __func__, __LINE__);
	memset(handle, 0, size);

	aac_lib = (aac_lib_t *)handle;

	//aac_lib->decoder.drc = &aac_lib->drc;
	
	NeAACDecoderInit(&aac_lib->decoder);
	//hDecoder.sample_buffer = pcm_buf;
	aac_lib->config = NeAACDecGetCurrentConfiguration(&aac_lib->decoder);

	aac_lib->config->defObjectType = LC;
	aac_lib->config->outputFormat = FAAD_FMT_16BIT;
	aac_lib->config->downMatrix = 0;
	aac_lib->config->useOldADTSFormat = 0;
	NeAACDecSetConfiguration(&aac_lib->decoder, aac_lib->config);

#ifdef MAIN_DEC
	for (i = 0; i < 8; i++)
	{
		reset_all_predictors(aac_lib->decoder.pred_stat[i], aac_lib->decoder.frameLength);
	}
#endif

	/*for (i = 0; i < MAX_CHANNELS; i++)
	{
		aac_lib->decoder.time_out[i] = aac_lib->time_out[i];
	}*/

	return 0;
}

static int aud_aac_release(void *handle)
{
	aac_lib_t *aac_lib = NULL;

	if (NULL == handle)
	{
		printf("<%s:%d> invalid parameter! [0x%p]\n", __func__, __LINE__, handle);
		return -1;
	}

	aac_lib = (aac_lib_t *)handle;
	
	NeAACDecoderRelease(&aac_lib->decoder);

	return 0;
}


char g_buf[FAAD_MIN_STREAMSIZE * MAX_CHANNELS];

static int aac_decode(int fd_input, char *outfile)
{
	int offset;
	int size, size_in_buf = 0, read_over = 0;
	int bread, first_time = 1;
	unsigned long samplerate;
	unsigned char channels;
	void *sample_buffer;
	audio_file *aufile = NULL;
	char *pcm_buf;

	printf("sizeof(aac_lib_t):%ld\n", sizeof(aac_lib_t));
	aac_lib_t *aac_lib = (aac_lib_t *)malloc(sizeof(aac_lib_t));
	aud_aac_init(aac_lib, sizeof(aac_lib_t), 0);

	size = read(fd_input, g_buf, sizeof(g_buf));
	if (size != sizeof(g_buf))
	{
		printf("read aac file error!\n");
		return -1;
	}
	size_in_buf = size;
	bread = NeAACDecInit(&aac_lib->decoder, g_buf, size, &samplerate, &channels);
	if (bread < 0)
	{
		printf("NeAACDecInit error!\n");
		return -1;
	}

	//printf("sample_rate:%d ch:%d\n", samplerate, channels);
	
	pcm_buf = malloc(PCM_BUF_SIZE);
	if (NULL == pcm_buf)
	{
		printf("malloc pcm_buf error!\n");
		return -1;
	}

	//adts_parse(g_buf, size_in_buf);

	while (1)
	{
		offset = aac_adts_parse(g_buf, size_in_buf);
		if (offset < 0)
		{
			printf("aac_adts_parse error!\n");
		}
		size_in_buf -= offset;
		memmove(g_buf, g_buf + offset, size_in_buf);
	
		sample_buffer = NeAACDecDecode2(&aac_lib->decoder, &aac_lib->frame_info, g_buf, g_frm_len, (void **)&pcm_buf, PCM_BUF_SIZE);
		//sample_buffer = NeAACDecDecode(&hDecoder, &frameInfo, g_buf, size_in_buf);
		size_in_buf -= aac_lib->frame_info.bytesconsumed;
		memmove(g_buf, g_buf + aac_lib->frame_info.bytesconsumed, size_in_buf);

		//printf("frm_len:%d used:%d sample_rate:%d/%d sample_num:%d\n", g_frm_len, frameInfo.bytesconsumed, 
		//	g_sample_rate, frameInfo.samplerate, frameInfo.samples);

		if (first_time)
		{
			aufile = open_audio_file(outfile, aac_lib->frame_info.samplerate, aac_lib->frame_info.channels,
				aac_lib->config->outputFormat, 1, aacChannelConfig2wavexChannelMask(&aac_lib->frame_info));
			if (aufile == NULL)
			{
				printf("open_audio_file error!\n");
				return -1;
			}
			first_time = 0;
		}

		if ((aac_lib->frame_info.error == 0) && (aac_lib->frame_info.samples > 0))
		{
			//printf("samples0:%d\n", aac_lib->frame_info.samples);
			if (write_audio_file(aufile, sample_buffer, aac_lib->frame_info.samples, 0) == 0)
			{
				printf("write_audio_file error!\n");
				break;
			}
		}

		if (size_in_buf <= 0)
		{
			printf("decode over!\n");
			break;
		}

		if (0 == read_over)
		{
			size = read(fd_input, g_buf + size_in_buf, sizeof(g_buf) - size_in_buf);
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

			size_in_buf += size;
		}
	}

	aud_aac_release(aac_lib);

	return 0;
}

int main(int argc, char *argv[])
{
	int fd_input, fd_output;
	
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

	/*fd_output = open(argv[2], O_WRONLY | O_CREAT, 0x666);
	if (fd_output <= 0)
	{
		printf("open %s error!\n", argv[2]);
		return -1;
	}*/

	// aac_init();

	aac_decode(fd_input, argv[2]);

	// aac_release();

	close(fd_input);
	//close(fd_output);
	
	return 0;
}

