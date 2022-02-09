/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggVorbis SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE OggVorbis SOURCE CODE IS (C) COPYRIGHT 1994-2009             *
 * by the Xiph.Org Foundation https://xiph.org/                     *
 *                                                                  *
 ********************************************************************

 function: simple example decoder

 ********************************************************************/

/* Takes a vorbis bitstream from stdin and writes raw stereo PCM to
   stdout. Decodes simple and chained OggVorbis files from beginning
   to end. Vorbisfile.a is somewhat more complex than the code below.  */

/* Note that this is POSIX, not ANSI code */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vorbis/codec.h>

ogg_int16_t convbuffer[4096]; /* take 8k out of the data segment, not the stack */
int convsize = 4096;

enum ogg_decode_state
{
	OGG_STATE_INPUT,
	OGG_STATE_DECODE
};

struct ogg_lib
{
	ogg_sync_state	 oy; /* sync and verify incoming physical bitstream */
	ogg_stream_state os; /* take physical pages, weld into a logical
						  stream of packets */
	ogg_page		 og; /* one Ogg bitstream page. Vorbis packets are inside */
	ogg_packet		 op; /* one raw packet of data for decode */

	vorbis_info 	 vi; /* struct that stores all the static vorbis bitstream
						  settings */
	vorbis_comment	 vc; /* struct that stores all the bitstream user comments */
	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
	vorbis_block	 vb; /* local working space for packet->PCM decode */
	
	int ogg_header_init_flag;
	int sam_num;
	enum ogg_decode_state decode_state;	// 0:input 1:decode
};

static struct ogg_lib ogg_lib;


FILE *output_file = NULL;

int ogg_init(struct ogg_lib* ogg_lib)
{
	ogg_lib->decode_state = OGG_STATE_INPUT;
	ogg_lib->ogg_header_init_flag = 0;
	ogg_sync_init(&ogg_lib->oy); /* Now we can read pages */
	
	return 0;
}

static int ogg_header_init(struct ogg_lib* ogg_lib)
{
	int ret;
	char **ptr;
	
	if (0 == ogg_lib->ogg_header_init_flag)
	{
		ret = ogg_sync_pageout(&ogg_lib->oy, &ogg_lib->og);
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
		ogg_stream_init(&ogg_lib->os, ogg_page_serialno(&ogg_lib->og));

		/** 
		extract the initial header from the first page and verify that the
		Ogg bitstream is in fact Vorbis data

		I handle the initial header first instead of just having the code
		read all three Vorbis headers at once because reading the initial
		header is an easy way to identify a Vorbis bitstream and it's
		useful to see that functionality seperated out.
		*/
		vorbis_info_init(&ogg_lib->vi);
		vorbis_comment_init(&ogg_lib->vc);

		if (ogg_stream_pagein(&ogg_lib->os, &ogg_lib->og) < 0)
		{
			/* error; stream version mismatch perhaps */
			fprintf(stderr, "Error reading first page of Ogg bitstream data.\n");
			return -1;
		}

		if (ogg_stream_packetout(&ogg_lib->os, &ogg_lib->op) != 1)
		{
			/* no page? must not be vorbis */
			fprintf(stderr, "Error reading initial header packet.\n");
			return -1;
		}

		if (vorbis_synthesis_headerin(&ogg_lib->vi, &ogg_lib->vc, &ogg_lib->op) < 0)
		{
			/* error case; not a vorbis header */
			fprintf(stderr, "This Ogg bitstream does not contain Vorbis "
					"audio data.\n");
			return -1;
		}

		ogg_lib->ogg_header_init_flag = 1;
	}

	if (1 == ogg_lib->ogg_header_init_flag)
	{
		static int init_cnt = 0;
		ret = ogg_sync_pageout(&ogg_lib->oy, &ogg_lib->og);
		if (ret == 0)
		{
			return 1;    /* Need more data */
		}

		/**
		 Don't complain about missing or corrupt data yet. We'll
		 catch it at the packet output phase 
		*/
		if (ret == 1)
		{
			ogg_stream_pagein(&ogg_lib->os, &ogg_lib->og); /* we can ignore any errors here
											as they'll also become apparent
											at packetout */
			while (init_cnt < 2)
			{
				ret = ogg_stream_packetout(&ogg_lib->os, &ogg_lib->op);
				if (ret == 0)
				{
					return 1;		/* Need more data */
				}

				if (ret < 0)
				{
					/* Uh oh; data at some point was corrupted or missing!
					   We can't tolerate that in a header.  Die. */
					fprintf(stderr, "Corrupt secondary header.  Exiting.\n");
					return -1;
				}

				ret = vorbis_synthesis_headerin(&ogg_lib->vi, &ogg_lib->vc, &ogg_lib->op);
				if (ret < 0)
				{
					fprintf(stderr, "Corrupt secondary header.  Exiting.\n");
					return -1;
				}

				init_cnt++;
			}
		}
		
		/* Throw the comments plus a few lines about the bitstream we're decoding */
		ptr = ogg_lib->vc.user_comments;
		while (*ptr)
		{
			fprintf(stderr, "%s\n", *ptr);
			++ptr;
		}

		fprintf(stderr, "\nBitstream is %d channel, %ldHz\n", ogg_lib->vi.channels, ogg_lib->vi.rate);
		fprintf(stderr, "Encoded by: %s\n\n", ogg_lib->vc.vendor);
		   
		init_cnt= 0;
		ogg_lib->ogg_header_init_flag = 2;
	}

	if (2 == ogg_lib->ogg_header_init_flag)
	{
		/* OK, got and parsed all three headers. Initialize the Vorbis
	   packet->PCM decoder. */
		if (vorbis_synthesis_init(&ogg_lib->vd, &ogg_lib->vi) == 0) /* central decode state */
		{
			/**
			 local state for most of the decode so multiple block decodes can
			 proceed in parallel. We could init multiple vorbis_block structures
			 for vd here 
			*/
			vorbis_block_init(&ogg_lib->vd, &ogg_lib->vb);      
			ogg_lib->ogg_header_init_flag = 3;
		}
	}

	if (3 != ogg_lib->ogg_header_init_flag)
	{
		fprintf(stderr, "Error: Corrupt header during playback initialization.\n");
		return -1;
	}

	return 0;
}

int total_samples = 0;



static int ogg_output_pcm(struct ogg_lib* ogg_lib)
{
	float **pcm;
	int samples;
	
	if (NULL == ogg_lib)
	{
		printf("<%s:%d> Invalid parameter,[%p]\n", __func__, __LINE__, ogg_lib);
		return -1;
	}

	
	/**
	pcm is a multichannel float vector.  In stereo, for
	example, pcm[0] is left, and pcm[1] is right.  samples is
	the size of each channel.  Convert the float values
	(-1.<=range<=1.) to whatever PCM format and write it out 
	*/
	while ((samples = vorbis_synthesis_pcmout(&ogg_lib->vd, &pcm)) > 0)
	{
		int i, j;
		int clipflag = 0;
		int bout = (samples < convsize ? samples : convsize);
		ogg_lib->sam_num += bout;
		/* convert floats to 16 bit signed ints (host order) and
		   interleave */
		for (i = 0; i < ogg_lib->vi.channels; i++)
		{
			ogg_int16_t *ptr = convbuffer + i;
			float *mono = pcm[i];
			//int *mono = pcm[i];		// ah test

			for (j = 0; j < bout; j++)
			{
#if 1
				//int val = floor(mono[j] * 32767.f + .5f);
				int val = mono[j];
				val = (val << 1);
#else /* optional dither */
				int val = mono[j] * 32767.f + drand48() - 0.5f;
#endif

				/* might as well guard against clipping */
				if (val > 32767)
				{
					val = 32767;
					clipflag = 1;
				}

				if (val < -32768)
				{
					val = -32768;
					clipflag = 1;
				}

				*ptr = val;
				ptr += ogg_lib->vi.channels;
			}
		}

		if (clipflag)
		{
			fprintf(stderr, "Clipping in frame %ld\n", (long)(ogg_lib->vd.sequence));
		}

		fwrite(convbuffer, 2 * ogg_lib->vi.channels, bout, output_file);

		/* tell libvorbis how many samples we actually consumed */
		vorbis_synthesis_read(&ogg_lib->vd, bout);
	}

	//printf("sam_num0:%d\n", ogg_lib->sam_num);
	return 0;
}


static int ogg_decode(struct ogg_lib* ogg_lib,char* src, int size)
{
	char *buffer;
	int ret;
	
	if ((NULL == ogg_lib) || (NULL == src) || (size <= 0))
	{
		printf("Invalid parameter [%p/%p/%d]\n", ogg_lib, src, size);
		return -1;
	}

	/*if (size < 4096)
	{
		printf("L%d need more data!\n", __LINE__);
		return 1;
	}*/

	if (OGG_STATE_INPUT == ogg_lib->decode_state)
	{
		buffer = ogg_sync_buffer(&ogg_lib->oy, size/*4096*/);
		if (buffer)
		{
			memcpy(buffer, src, size);
		}
		else
		{
			printf("L:%d ogg_sync_buffer return NULL!\n", __LINE__);
			return -1;
		}
		ogg_sync_wrote(&ogg_lib->oy, size);

		if (3 != ogg_lib->ogg_header_init_flag)
		{
			ret = ogg_header_init(ogg_lib);
			if (0 != ret)
			{
				return ret;
			}
		}

		convsize = 4096 / ogg_lib->vi.channels;		// ?????

		ret = ogg_sync_pageout(&ogg_lib->oy, &ogg_lib->og);
		if (ret == 0)
		{
			return 1;    /* need more data */
		}

		if (ret < 0) /* missing or corrupt data at this page position */
		{
			fprintf(stderr, "Corrupt or missing data in bitstream; "
			        "continuing...\n");
			return -1;
		}
		else
		{
			//printf("<%s:%d>\n", __func__, __LINE__);
			ogg_stream_pagein(&ogg_lib->os, &ogg_lib->og); /* can safely ignore errors at this point */
			ogg_lib->decode_state = OGG_STATE_DECODE;
		}
	}

	ogg_lib->sam_num = 0;

	if (OGG_STATE_DECODE == ogg_lib->decode_state)
	{
		ret = ogg_stream_packetout(&ogg_lib->os, &ogg_lib->op);
		if (ret == 0)
		{
			//printf("more\n");
			ogg_lib->decode_state = OGG_STATE_INPUT;
			return 1;	 /* need more data */
		}

		if (ret < 0) /* missing or corrupt data at this page position */
		{
			/* no reason to complain; already complained above */
		}
		else
		{
			/* we have a packet.  Decode it */
			if (vorbis_synthesis(&ogg_lib->vb, &ogg_lib->op) == 0) /* test for success! */
			{
				vorbis_synthesis_blockin(&ogg_lib->vd, &ogg_lib->vb);
			}

			ogg_output_pcm(ogg_lib);
		}
	}

	if (ogg_page_eos(&ogg_lib->og))
	{
		printf("L%d ogg file eos!\n", __LINE__);
		//eos = 1;
	}
	
	return 0;
}

int ogg_deinit(struct ogg_lib* ogg_lib)
{
	ogg_stream_clear(&ogg_lib->os);
	vorbis_comment_clear(&ogg_lib->vc);
	vorbis_info_clear(&ogg_lib->vi);  /* must be called last */

	/* OK, clean up the framer */
	ogg_sync_clear(&ogg_lib->oy);

	return 0;
}

int main(int argc, char *argv[])
{
	char src[4096];
	int read_size = 0;
	int ret, i=0;
	FILE *input_file;
	
	if (argc < 3)
	{
		printf("usage %s input_file output_file!\n", argv[0]);
		return 0;
	}
	else
	{
		int i;
		for (i = 0; i < argc; i++)
		{
			printf("argv[%d]:%s\n", i, argv[i]);
		}
	}

	input_file = fopen(argv[1], "r");
	if (NULL == input_file)
	{
		printf("open file:%s error!\n", argv[1]);
		return -1;
	}

	output_file = fopen(argv[2], "w+");
	if (NULL == output_file)
	{
		printf("open file:%s error!\n", argv[2]);
		fclose(input_file);
		return -1;
	}

	/********** Decode setup ************/
	ogg_init(&ogg_lib);

	while (1)
	{
		///printf("read\n");
		if (OGG_STATE_INPUT == ogg_lib.decode_state)
		{
			read_size = fread(src, 1, 4096, input_file);
			if (read_size == 0)
			{
				printf("read file end!\n");
				break;
			}
		}

		/*i++;
		if (i == 10 || 14 == i)
		{
			printf("skip %d byte data!\n", read_size);
			continue;
		}*/

		ret = ogg_decode(&ogg_lib, src, read_size);
		if (ret < 0)
		{
			printf("L%d decode error!\n", __LINE__);
		}

		if (total_samples)
		{
			printf("samples:%d\n", total_samples);
			total_samples = 0;
		}
	}

	/* clean up this logical bitstream; before exit we see if we're
	   followed by another [chained] */
	ogg_deinit(&ogg_lib);

	fprintf(stderr, "Done.\n");
	return (0);
}
