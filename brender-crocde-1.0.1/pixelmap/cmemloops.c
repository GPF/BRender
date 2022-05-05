/**
 * Inner loops for operations on in-memory pixelmaps.
 * Ported from memloops.asm
 */
#include "brender.h"
#include "brassert.h"
#include <string.h>

#include <stdio.h>

br_uint_8 bit_to_mask_s[] =
{
	0xFF,	/* 0b11111111 */
	0x7F,	/* 0b01111111 */
	0x3F,	/* 0b00111111 */
	0x1F,	/* 0b00011111 */
	0x0F,	/* 0b00001111 */
	0x07,	/* 0b00000111 */
	0x03,	/* 0b00000011 */
	0x01,	/* 0b00000001 */
	0x00	/* 0b00000000 */
};

br_uint_8 bit_to_mask_e[] =
{
	0x00,	/* 0b00000000 */
	0x80,	/* 0b10000000 */
	0xC0,	/* 0b11000000 */
	0xE0,	/* 0b11100000 */
	0xF0,	/* 0b11110000 */
	0xF8,	/* 0b11111000 */
	0xFC,	/* 0b11111100 */
	0xFE,   /* 0b11111110 */
	0xFF    /* 0b11111111 */
};

static br_uint_32 *MemFill32(br_uint_32 *dest, br_uint_32 value, br_uint_32 num)
{
	while(num--)
		*(dest++) = value;

	return dest;
}

static br_uint_16 *MemFill16(br_uint_16 *dest, br_uint_16 value, br_uint_32 num)
{
	while(num--)
		*(dest++) = value;

	return dest;
}

static br_uint_8 *MemFill8(br_uint_8 *dest, br_uint_8 value, br_uint_32 num)
{
	memset(dest, value, num);

	return dest + num;
}

static void bc_inner_loop(char *dest, br_uint_32 bpp, br_uint_32 colour, br_uint_8 c)
{
	br_int_16 carry_check;
	br_uint_32 i;
	br_uint_8 oldC;

	for(i = 0; i < 8; ++i)
	{
		/* Carry check is working, don't touch it. */
		oldC = c;
		c += c;
		if((carry_check = oldC + oldC) <= 255)
			continue;

		switch(bpp)
		{
			case 1:
				dest[(i * 1) + 0] = (br_uint_8)(colour & 0xFF);						/* mov [edi+(x*1)],bl */
				break;
			case 2:
				dest[(i * 2) + 0] = (br_uint_8)(colour & 0x00FF);					/* mov [edi+(x*2)+0],bl */
				dest[(i * 2) + 1] = (br_uint_8)((colour & 0xFF00) >> 8);				/* mov [edi+(x*2)+1],bh */
				break;
			case 3:
				dest[(i * 3) + 0] = (br_uint_8)(colour & 0x0000FF);					/* mov [edi+(x*3)+0],bl */
				dest[(i * 3) + 1] = (br_uint_8)((colour & 0x00FF00) >> 8);			/* mov [edi+(x*3)+1],bh */
				dest[(i * 3) + 2] = (br_uint_8)(((colour >> 8) & 0x00FF00) >> 8);		/* mov [edi+(x*3)+2],ah */
				break;
			case 4:
				*((br_uint_32*)(&dest[(i * 4) + 0])) = colour;						/* mov [edi+(x*4)],ebx */
				break;
		}
	}
}

static void COPY_BITS_CORE_1(char *dest, br_uint_32 d_stride, br_uint_8 *src, br_uint_32 s_stride,
							 br_uint_32 nrows, br_uint_32 bpp, br_uint_32 colour, br_uint_8 himask, br_uint_8 lomask)
{
	br_uint_8 comb_mask = himask & lomask;

	for(; nrows; --nrows, dest += d_stride, src += s_stride)
	{
		/* We're 1 pixel wide, loop vertically (i.e. through each row) */
		bc_inner_loop(dest, bpp, colour, (br_uint_8)(*src & comb_mask));
	}
}

static void COPY_BITS_CORE_N(char *dest, br_int_32 d_stride, br_uint_8 *src, br_uint_32 s_stride,
							 br_uint_32 nrows, br_uint_32 bpp, br_uint_32 colour, br_uint_8 himask, br_uint_8 lomask)
{
	char c;
	br_int_32 bppt = (br_int_32)(bpp << 3);
	do
	{
		c = *(src)& lomask;

h_loop:
		++src;
h_loop_last:
		// Set destination pixels according to byte mask
		bc_inner_loop(dest, bpp, colour, c);

		c = *src;
		dest += 8 * bpp;
		--bppt;

		if(bppt > 0)
			goto h_loop;
		if(bppt < 0)
			goto done_row;

		// If it is the last byte, mask and look one final time
		c &= lomask;
		goto h_loop_last;

done_row:
		dest += d_stride;
		src += s_stride;
		--nrows;
	}
	while(nrows);
}

void _MemCopyBits_A(char *dest, br_int_32 d_stride,
					br_uint_8 *src, br_uint_32 s_stride,
					br_uint_32 start_bit, br_uint_32 end_bit,
					br_uint_32 nrows, br_uint_32 bpp, br_uint_32 colour)
{
	br_uint_8 himask = bit_to_mask_e[end_bit & 7];
	br_uint_8 lomask = bit_to_mask_s[start_bit];

	if((end_bit >> 3) && (bpp >= 1 && bpp <= 4))
		COPY_BITS_CORE_N(dest, d_stride, src, s_stride, nrows, bpp, colour, himask, lomask);
	else if(bpp >= 1 && bpp <= 4)
		COPY_BITS_CORE_1(dest, d_stride, src, s_stride, nrows, bpp, colour, himask, lomask);
}



void _MemFill_A(char *dest, br_uint_32 pixels, br_uint_32 bpp, br_uint_32 colour)
{
	/* 32-bit */
	if(bpp == 4)
	{
		MemFill32((br_uint_32*)dest, colour, pixels);
	}
	/* 16-bit */
	else if(bpp == 2)
	{
		MemFill16((br_uint_16*)dest, (br_uint_16)colour, pixels);

	}
	/* 24-bit */
	else if(bpp == 3)
	{
		char *cols = (char*)&colour;

		/* If 3 fill bytes are the same, just use 1bpp fill */
		if((cols[0] == cols[1]) && (cols[0] == cols[2]))
		{
			_MemFill_A(dest, pixels, 1, colour);
		}
		else for(br_uint_32 i = 0; i < pixels; ++i)
		{
			*dest++ = (colour & 0x00FF0000) >> 16;
			*dest++ = (colour & 0x0000FF00) >> 8;
			*dest++ = (colour & 0x000000FF) >> 0;
		}
	}
	else if(bpp == 1)
	{
		MemFill8(dest, (br_uint_8)(colour & 0xFF), pixels);
	}
}


void _MemRectFill_A(char *dest, br_uint_32 pwidth, br_uint_32 pheight,
					br_int_32 d_stride, br_uint_32 bpp, br_uint_32 colour)
{

	/* Called like:
	 *_MemRectFill_A(DevicePixelmapMemAddress(self,0,0,bytes),self->pm_pixels_qualifier,
	 * 		self->pm_width,self->pm_height, self->pm_row_bytes, bytes, colour);
	 */

	MemFill8((br_uint_8*)dest, (br_uint_8)colour, (d_stride*pwidth)*pheight);
}

void _MemRectCopy_A(char *dest, char *src,
					br_uint_32 pwidth, br_uint_32 pheight,
					br_int_32 d_stride, br_int_32 s_stride,
					br_uint_32 bpp)
{
	void *v10; // edi@1
	unsigned int v11; // ecx@1
	int v12; // edx@3
	void *v13; // [sp-14h] [bp-20h]@2
	char *v14; // [sp-10h] [bp-1Ch]@2
	unsigned int v15; // [sp-Ch] [bp-18h]@2

	v10 = dest;
	v11 = pwidth * bpp;
	do
	{
		v15 = v11;
		v14 = src;
		v13 = v10;
		if((unsigned __int8)v10 & 3)
		{
			v12 = 4 - ((unsigned __int8)v10 & 3);
			while(1)
			{
				*((br_uint_8*)v10) = *(src++);
				v10 = (char *)v10 + 1;
				--v11;
				if(!v11)
					break;
				--v12;
				if(!v12)
					goto LABEL_6;
			}
		}
		else
		{
LABEL_6:
			memcpy(v10, src, v11);
		}
		v11 = v15;
		v10 = (char *)v13 + d_stride;
		src = &v14[s_stride];
		--pheight;
	}
	while(pheight);
}

void _MemCopy_A(char *dest, char *src, br_uint_32 pixels, br_uint_32 bpp)
{
	char *v6; // esi@1
	void *v7; // edi@1
	char v8; // al@1
	unsigned int v9; // ecx@1
	int i; // edx@1
	char v11; // dl@3
	unsigned int v12; // ecx@3
	int v13; // edi@3
	int j; // ecx@3

	v6 = src;
	v7 = dest;
	v8 = pixels * bpp;
	v9 = pixels * bpp;
	for(i = (unsigned __int8)dest & 3; i; --i)
	{
		v8 = *v6;
		*((br_uint_8*)v7) = *(v6++);
		v7 = (char *)v7 + 1;
		--v9;
	}
	v11 = v9;
	v12 = v9 >> 2;
	memcpy(v7, v6, 4 * v12);
	v13 = (int)((char *)v7 + 4 * v12);
	for(j = v11 & 3; j; --j)
		*((br_uint_8*)v13) = v8;
	++v13;
}

void _MemPixelSet(char *dest, br_uint_32 bytes, br_uint_32 colour)
{
	switch(bytes)
	{
		case 1:
			*((br_uint_8*)dest) = (br_uint_8)colour;
			break;
		case 2:
			*((br_uint_16*)dest) = (br_uint_16)colour;
			break;
		case 3:
			*dest = (br_uint_8)colour;
			*(dest + 1) = (br_uint_8)((colour & 0x00FF00) >> 8);
			*(dest + 2) = (br_uint_8)((colour & 0xFF0000) >> 16);
			break;
		case 4:
			*((br_uint_32*)dest) = colour;
			break;
	}
}

br_uint_32 _MemPixelGet(char *dest, br_uint_32 bytes)
{
	br_uint_32 result = 0;

	switch(bytes)
	{
		case 1:
			result = *dest;
			break;
		case 2:
			result = *((br_uint_16*)dest);
			break;
		case 3:
			result = (*(dest + 2) << 16) | (*(dest + 1) << 8) | (*dest);
			break;
		case 4:
			result = *((br_uint_32*)dest);
			break;
	}

	return result;
}
