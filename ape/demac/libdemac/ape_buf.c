#include <ape/ape_buf.h>

int ape_buf_init(ape_buf_t *ape_buf, int size)
{
	if ((NULL == ape_buf) || (size <= 0))
	{
		return -1;
	}

	ape_buf->data = malloc(size);
	if (NULL == ape_buf->data)
	{
		printf("<%s:%d> malloc ape_buf error!\n", __func__, __LINE__);
		return -1;
	}

	ape_buf->rd = ape_buf->wt = 0;
	ape_buf->size = size;

	ape_buf->rd_size = ape_buf->wt_size = 0;

	return 0;
}

int ape_buf_release(ape_buf_t *ape_buf)
{
	if (NULL == ape_buf)
	{
		return -1;
	}

	if (NULL != ape_buf->data)
	{
		free(ape_buf->data);
		ape_buf->data = NULL;
	}

	ape_buf->rd = ape_buf->wt = 0;
	ape_buf->size = 0;

	ape_buf->rd_size = ape_buf->wt_size = 0;

	return 0;
}

int ape_buf_remain_data(ape_buf_t *ape_buf)
{
	int remain_data;
	
	if ((NULL == ape_buf) || (NULL == ape_buf->data))
	{
		return -1;
	}

	if (ape_buf->wt >= ape_buf->rd)
	{
		remain_data = ape_buf->wt - ape_buf->rd;
	}
	else
	{
		remain_data = ape_buf->size - ape_buf->rd + ape_buf->wt;
	}

	return remain_data;
}

int ape_buf_remain_space(ape_buf_t *ape_buf)
{
	int remain_space;
	
	if ((NULL == ape_buf) || (NULL == ape_buf->data))
	{
		return -1;
	}

	if (ape_buf->wt >= ape_buf->rd)
	{
		remain_space = ape_buf->size - ape_buf->wt + ape_buf->rd;
	}
	else
	{
		remain_space = ape_buf->rd - ape_buf->wt;
	}

	return remain_space;
}

/*return actual write size*/
int ape_buf_write(ape_buf_t *ape_buf, const char *buf, int size)
{
	int high_space;
	int remain_space;
	
	if ((NULL == ape_buf) || (NULL == ape_buf->data) || (NULL == buf) || (size <= 0))
	{
		return -1;
	}

	remain_space = ape_buf_remain_space(ape_buf);
	if (size > (remain_space - 1))
	{
		size = remain_space - 1;
	}

	if (size > 0)
	{
		if (size > (ape_buf->size - ape_buf->wt))
		{
			ape_buf_reset(ape_buf);
		}
		
		if (ape_buf->wt >= ape_buf->rd)
		{
			high_space = ape_buf->size - ape_buf->wt;
			if (high_space >= size)
			{
				memcpy(ape_buf->data + ape_buf->wt, buf, size);
				ape_buf->wt += size;
			}
			else
			{
				memcpy(ape_buf->data + ape_buf->wt, buf, high_space);
				memcpy(ape_buf->data, buf + high_space, size - high_space);
				ape_buf->wt = size - high_space;
			}
		}
		else	// wt < rd
		{
			memcpy(ape_buf->data + ape_buf->wt, buf, size);
			ape_buf->wt += size;
		}

		if (ape_buf->wt == ape_buf->size)
		{
			ape_buf->wt = 0;
		}
	}

	ape_buf->wt_size += size;
	
	return size;
}

/*return actual update write size*/
int ape_buf_update_write_point(ape_buf_t *ape_buf, int size)
{
	int high_space;
	int remain_space;
	
	if ((NULL == ape_buf) || (NULL == ape_buf->data) || (size <= 0))
	{
		return -1;
	}

	remain_space = ape_buf_remain_space(ape_buf);
	if (size > (remain_space - 1))
	{
		size = remain_space - 1;
	}

	if (size > 0)
	{
		if (size > (ape_buf->size - ape_buf->wt))
		{
			ape_buf_reset(ape_buf);
		}
		
		if (ape_buf->wt >= ape_buf->rd)
		{
			high_space = ape_buf->size - ape_buf->wt;
			if (high_space >= size)
			{
				ape_buf->wt += size;
			}
			else
			{
				ape_buf->wt = size - high_space;
			}
		}
		else	// wt < rd
		{
			ape_buf->wt += size;
		}

		if (ape_buf->wt == ape_buf->size)
		{
			ape_buf->wt = 0;
		}
	}

	ape_buf->wt_size += size;
	
	return size;
}


/*return actual read size*/
int ape_buf_read(ape_buf_t *ape_buf, char *buf, int size)
{
	int high_data;
	int remain_data;
	
	if ((NULL == ape_buf) || (NULL == ape_buf->data) || (NULL == buf) || (size <= 0))
	{
		return -1;
	}

	remain_data = ape_buf_remain_data(ape_buf);
	if (size > remain_data)
	{
		size = remain_data;
	}

	if (size > 0)
	{
		if (ape_buf->wt > ape_buf->rd)
		{
			memcpy(buf, ape_buf->data + ape_buf->rd, size);
			ape_buf->rd += size;
		}
		else	// wt < rd
		{
			high_data = ape_buf->size - ape_buf->rd;
			if (high_data >= size)
			{
				memcpy(buf, ape_buf->data + ape_buf->rd, size);
				ape_buf->rd += size;
			}
			else
			{
				memcpy(buf, ape_buf->data + ape_buf->rd, high_data);
				memcpy(buf + high_data, ape_buf->data, size - high_data);
				ape_buf->rd = size - high_data;
			}
		}

		if (ape_buf->rd == ape_buf->size)
		{
			ape_buf->rd = 0;
		}
	}

	ape_buf->rd_size += size;
	
	return size;
}

/*return actual read size, not update rd point*/
int ape_buf_try_read(ape_buf_t *ape_buf, char *buf, int size)
{
	int high_data;
	int remain_data;
	
	if ((NULL == ape_buf) || (NULL == ape_buf->data) || (NULL == buf) || (size <= 0))
	{
		return -1;
	}

	remain_data = ape_buf_remain_data(ape_buf);
	if (size > remain_data)
	{
		size = remain_data;
	}

	if (size > 0)
	{
		if (ape_buf->wt > ape_buf->rd)
		{
			memcpy(buf, ape_buf->data + ape_buf->rd, size);
		}
		else	// wt < rd
		{
			high_data = ape_buf->size - ape_buf->rd;
			if (high_data >= size)
			{
				memcpy(buf, ape_buf->data + ape_buf->rd, size);
			}
			else
			{
				memcpy(buf, ape_buf->data + ape_buf->rd, high_data);
				memcpy(buf + high_data, ape_buf->data, size - high_data);
			}
		}
	}
	
	return size;
}

/*return actual update size*/
int ape_buf_update_read_point(ape_buf_t *ape_buf, int size)
{
	int remain_data, high_data;
	
	if ((NULL == ape_buf) || (NULL == ape_buf->data) || (size <= 0))
	{
		return -1;
	}

	remain_data = ape_buf_remain_data(ape_buf);
	if (size > remain_data)
	{
		size = remain_data;
	}

	if (size > 0)
	{
		if (ape_buf->wt > ape_buf->rd)
		{
			ape_buf->rd += size;
		}
		else	// wt < rd
		{
			high_data = ape_buf->size - ape_buf->rd;
			if (high_data >= size)
			{
				ape_buf->rd += size;
			}
			else
			{
				ape_buf->rd = size - high_data;
			}
		}

		if (ape_buf->rd == ape_buf->size)
		{
			ape_buf->rd = 0;
		}
	}

	ape_buf->rd_size += size;

	return size;
}

/*return actual skip size*/
int ape_buf_skip_data(ape_buf_t *ape_buf, int size)
{
	return ape_buf_update_read_point(ape_buf, size);
}

unsigned int ape_buf_read_u8(ape_buf_t *ape_buf)
{
	unsigned int val;
	
	if (NULL == ape_buf)
	{
		return -1;
	}

	if (ape_buf_remain_data(ape_buf) < 1)
	{
		return -1;
	}

	val = (unsigned char)ape_buf->data[ape_buf->rd];
	ape_buf->rd++;
	if (ape_buf->rd == ape_buf->size)
	{
		ape_buf->rd = 0;
	}
	
	ape_buf->rd_size += 1;
	
	return val;
}

unsigned int ape_buf_read_u16(ape_buf_t *ape_buf)
{
	unsigned int val;
	
	if (NULL == ape_buf)
	{
		return -1;
	}

	if (ape_buf_remain_data(ape_buf) < 2)
	{
		return -1;
	}

	val = ape_buf_read_u8(ape_buf);
	val |= (ape_buf_read_u8(ape_buf) << 8);
	
	return val;
}

unsigned int ape_buf_read_u32(ape_buf_t *ape_buf)
{
	unsigned int val;
	
	if (NULL == ape_buf)
	{
		return -1;
	}

	if (ape_buf_remain_data(ape_buf) < 4)
	{
		return -1;
	}

	val = ape_buf_read_u16(ape_buf);
	val |= (ape_buf_read_u16(ape_buf) << 16);
	
	return val;
}


unsigned int ape_buf_try_read_u8(ape_buf_t *ape_buf)
{
	unsigned int val;
	
	if (NULL == ape_buf)
	{
		return -1;
	}

	if (ape_buf_remain_data(ape_buf) < 1)
	{
		return -1;
	}

	val = (unsigned char)ape_buf->data[ape_buf->rd];
	
	return val;
}

unsigned int ape_buf_try_read_u16(ape_buf_t *ape_buf)
{
	unsigned int val;
	
	if (NULL == ape_buf)
	{
		return -1;
	}

	if (ape_buf_remain_data(ape_buf) < 2)
	{
		return -1;
	}

	val = (unsigned char)ape_buf->data[ape_buf->rd];
	val |= ((unsigned char)ape_buf->data[ape_buf->rd + 1] << 8);
	
	return val;
}

unsigned int ape_buf_try_read_u32(ape_buf_t *ape_buf)
{
	unsigned int val;
	
	if (NULL == ape_buf)
	{
		return -1;
	}

	if (ape_buf_remain_data(ape_buf) < 4)
	{
		return -1;
	}

	val = (unsigned char)ape_buf->data[ape_buf->rd];
	val |= ((unsigned char)ape_buf->data[ape_buf->rd + 1] << 8);
	val |= ((unsigned char)ape_buf->data[ape_buf->rd + 2] << 16);
	val |= ((unsigned char)ape_buf->data[ape_buf->rd + 3] << 24);
	
	return val;
}

unsigned int ape_buf_total_read(ape_buf_t *ape_buf)
{
	if (NULL == ape_buf)
	{
		return -1;
	}

	return ape_buf->rd_size;
}

unsigned int ape_buf_total_write(ape_buf_t *ape_buf)
{
	if (NULL == ape_buf)
	{
		return -1;
	}

	return ape_buf->wt_size;
}

unsigned int ape_buf_reset(ape_buf_t *ape_buf)
{
	char *tmp_buf = NULL;
	int remain_data, low_size, hi_size;
	
	if ((NULL == ape_buf) || (NULL == ape_buf->data))
	{
		return -1;
	}

	remain_data = ape_buf_remain_data(ape_buf);

	if (0 == ape_buf->rd)
	{
		return 0;
	}

	if (ape_buf->rd == ape_buf->wt)		// empty
	{
		return 0;
	}
	else if (ape_buf->rd < ape_buf->wt)
	{
		memmove(ape_buf->data, ape_buf->data + ape_buf->rd, remain_data);
		ape_buf->wt = remain_data;
		ape_buf->rd = 0;
	}
	else
	{
		tmp_buf = malloc(ape_buf->wt);
		if (NULL == tmp_buf)
		{
			printf("<%s:%d> malloc tmp_buf error!\n", __func__, __LINE__);
			return -1;
		}

		low_size = ape_buf->wt;
		hi_size = ape_buf->size - ape_buf->rd;
		memcpy(tmp_buf, ape_buf->data, low_size);
		memmove(ape_buf->data, ape_buf->data + ape_buf->rd, hi_size);
		memcpy(ape_buf->data + hi_size, tmp_buf, low_size);
		ape_buf->rd = 0;
		ape_buf->wt = remain_data;

		free(tmp_buf);
		tmp_buf = NULL;
	}

	return 0;
}

char *ape_buf_data_addr(ape_buf_t *ape_buf)
{
	if ((NULL == ape_buf) || (NULL == ape_buf->data))
	{
		return NULL;
	}

	return ape_buf->data + ape_buf->rd;
}

