#ifndef __APE_BUF_H__
#define __APE_BUF_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct ape_buf_s
{
	char *data;
	int rd;
	int wt;
	int size;

	unsigned int rd_size;
	unsigned int wt_size;
} ape_buf_t;

int ape_buf_init(ape_buf_t *ape_buf, int size);
int ape_buf_release(ape_buf_t *ape_buf);
int ape_buf_remain_data(ape_buf_t *ape_buf);
int ape_buf_remain_space(ape_buf_t *ape_buf);
int ape_buf_write(ape_buf_t *ape_buf, const char *buf, int size);
int ape_buf_update_write_point(ape_buf_t *ape_buf, int size);
int ape_buf_read(ape_buf_t *ape_buf, char *buf, int size);
int ape_buf_try_read(ape_buf_t *ape_buf, char *buf, int size);
int ape_buf_update_read_point(ape_buf_t *ape_buf, int size);
int ape_buf_skip_data(ape_buf_t *ape_buf, int size);
unsigned int ape_buf_read_u8(ape_buf_t *ape_buf);
unsigned int ape_buf_read_u16(ape_buf_t *ape_buf);
unsigned int ape_buf_read_u32(ape_buf_t *ape_buf);
unsigned int ape_buf_try_read_u8(ape_buf_t *ape_buf);
unsigned int ape_buf_try_read_u16(ape_buf_t *ape_buf);
unsigned int ape_buf_try_read_u32(ape_buf_t *ape_buf);
unsigned int ape_buf_total_read(ape_buf_t *ape_buf);
unsigned int ape_buf_total_write(ape_buf_t *ape_buf);
unsigned int ape_buf_reset(ape_buf_t *ape_buf);
char *ape_buf_data_addr(ape_buf_t *ape_buf);



#endif


