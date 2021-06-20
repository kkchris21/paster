/**
 * @brief  micros and structures for a simple PNG file 
 *
 * Copyright 2018-2020 Yiqing Huang
 *
 * This software may be freely redistributed under the terms of MIT License
 */
#pragma once

/******************************************************************************
 * INCLUDE HEADER FILES
 *****************************************************************************/
#include <stdio.h>

/******************************************************************************
 * DEFINED MACROS 
 *****************************************************************************/

#define PNG_SIG_SIZE    8 /* number of bytes of png image signature data */
#define CHUNK_LEN_SIZE  4 /* chunk length field size in bytes */          
#define CHUNK_TYPE_SIZE 4 /* chunk type field size in bytes */
#define CHUNK_CRC_SIZE  4 /* chunk CRC field size in bytes */
#define DATA_IHDR_SIZE 13 /* IHDR chunk data field size */

/******************************************************************************
 * STRUCTURES and TYPEDEFS 
 *****************************************************************************/
typedef unsigned char U8;
typedef unsigned int  U32;

typedef struct chunk {
    U32 length;  /* length of data in the chunk, host byte order */
    U8  type[4]; /* chunk type */
    U8  *p_data; /* pointer to location where the actual data are */
    U32 crc;     /* CRC field  */
} *chunk_p;

/* note that there are 13 Bytes valid data, compiler will padd 3 bytes to make
   the structure 16 Bytes due to alignment. So do not use the size of this
   structure as the actual data size, use 13 Bytes (i.e DATA_IHDR_SIZE macro).
 */
typedef struct data_IHDR {// IHDR chunk data 
    U32 width;        /* width in pixels, big endian   */
    U32 height;       /* height in pixels, big endian  */
    U8  bit_depth;    /* num of bits per sample or per palette index.
                         valid values are: 1, 2, 4, 8, 16 */
    U8  color_type;   /* =0: Grayscale; =2: Truecolor; =3 Indexed-color
                         =4: Greyscale with alpha; =6: Truecolor with alpha */
    U8  compression;  /* only method 0 is defined for now */
    U8  filter;       /* only method 0 is defined for now */
    U8  interlace;    /* =0: no interlace; =1: Adam7 interlace */
} *data_IHDR_p;

/* A simple PNG file format, three chunks only*/
typedef struct simple_PNG {
    struct chunk *p_IHDR;
    struct chunk *p_IDAT;  /* only handles one IDAT chunk */  
    struct chunk *p_IEND;
} *simple_PNG_p;

/******************************************************************************
 * FUNCTION PROTOTYPES 
 *****************************************************************************/
int is_png(U8 *buf, size_t n);
int get_png_height(struct data_IHDR *buf);
int get_png_width(struct data_IHDR *buf);
int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence);

/* declare your own functions prototypes here */
int is_png(U8 *buf, size_t n) {
    U8 magic_number[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    for (int i = 0; i<8; ++i) {
        if(*(buf+i) != magic_number[i]) {
            return 0;
        }
    }
    return 1;
}

void reverse(U8 arr[], int n)
{
    U8 buff[n];
    for (int i = 0; i < n; ++i) {
        buff[n - i - 1] = arr[i];
    }
    for (int i = 0; i < n; ++i) {
        arr[i] = buff[i];
    }
}

int get_png_data_IHDR(data_IHDR_p out, FILE *fp, long offset, int whence) {
    data_IHDR_p p_out = out;
    U8 buffer [13];
    fseek(fp, 16, SEEK_SET);
    fread(buffer, 13, 1, fp);
    reverse(buffer,4);
    U32* pInt = (U32*)buffer;
    p_out->width = *pInt;
    reverse(buffer,8);
    U32* pInt2 = (U32*)buffer;
    p_out->height = *pInt2;
    p_out->bit_depth = buffer[8];
    p_out->color_type = buffer[9];
    p_out->compression = buffer[10];
    p_out->filter = buffer[11];
    p_out->interlace = buffer[12];

    return 0;
}

int get_png_height(struct data_IHDR *buf) {
    return buf->height;
}
int get_png_width(struct data_IHDR *buf) {
    return buf->width;
}