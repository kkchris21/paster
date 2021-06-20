#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <getopt.h>
#include "crc.h"
#include "lab_png.h"
#include "zutil.h"

#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;


size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);


/**
 * @brief  cURL header call back function to extract image sequence number from 
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line 
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv, 
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}


int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }
    
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	return 1;
    }
    
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}


/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3; 
    }
    return fclose(fp);
}

void catpng() 
{
    FILE *fp;
    U8 buffer_4 [8];
    U8 magic_number[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    char number[7];
    for (int i = 0; i< 49; i++) {
    sprintf(number, "%d.png", i);
    fp =(i == 0)? fopen(number, "r") : fopen("all.png", "r");
    struct data_IHDR ihdr = { .width = 0, .height = 0, .bit_depth = 0, .color_type=0,.compression=0, .filter=0,.interlace=0 };
    data_IHDR_p out = &ihdr;
    get_png_data_IHDR(out, fp, 0, 0);
    int width1 = get_png_width(out);
    int height1 = get_png_height(out);
    
    fseek(fp,12,SEEK_SET);
    U8 ihdr_buffer_without_length [17];
    fread(ihdr_buffer_without_length, 17, 1, fp);

    fseek(fp,8,SEEK_SET);
    U8 ihdr_buffer [21];
    fread(ihdr_buffer, 21, 1, fp);

    fseek(fp,-12,SEEK_END);
    U8 iend_buffer [12];
    fread(iend_buffer, 12, 1, fp);

    fseek(fp,33,SEEK_SET);
    fread(buffer_4, 4, 1, fp);
    U32* pInt = (U32*)buffer_4;
    unsigned long length1 = htonl(*pInt);
    fseek(fp,41,SEEK_SET);
    U8* idat_buffer_data1 = malloc(length1* sizeof(U8));
    fread(idat_buffer_data1, length1, 1, fp);

    fseek(fp,37,SEEK_SET);
    U8* type_buffer = malloc((4)* sizeof(U8));
    fread(type_buffer, 4, 1, fp);

    int size1 = height1 * (width1 * 4 + 1);
    U8* gp_buf_inf1 = malloc((size1)* sizeof(U8));
    U64 len_inf1 = size1;
    mem_inf(gp_buf_inf1, &len_inf1, idat_buffer_data1, length1);
    fclose(fp);
    sprintf(number, "%d.png", i+1);
    fp = fopen(number, "r");
    get_png_data_IHDR(out, fp, 0, 0);
    int width2 = get_png_width(out);
    int height2 = get_png_height(out);

    fseek(fp,33,SEEK_SET);
    fread(buffer_4, 4, 1, fp);
    U32* pInt2 = (U32*)buffer_4;
    unsigned long length2 = htonl(*pInt2);
    fseek(fp,41,SEEK_SET);
    U8* idat_buffer_data2 = malloc(length2* sizeof(U8));
    fread(idat_buffer_data2, length2, 1, fp);
    int size2 = height2 * (width2 * 4 + 1);
    U8* gp_buf_inf2 = malloc((size2)* sizeof(U8));
    U64 len_inf2 = size2;
    mem_inf(gp_buf_inf2, &len_inf2, idat_buffer_data2, length2);
    fclose(fp);

    U32 height_sum = height1 + height2;
    U32 height_sum_convert = ntohl(height_sum);
    
    for (int i = 0; i < 4; ++i) {
        ihdr_buffer[15-i] = height_sum_convert >> (3 - i) * 8;
        ihdr_buffer_without_length[11-i] = height_sum_convert >> (3 - i) * 8;
    }

    unsigned long ihdr_crc = crc(ihdr_buffer_without_length,17);
    unsigned long ihdr_crc_convert = ntohl(ihdr_crc);


    U8 ihdr_crc_convert_pointer [4];

    for (int i = 0; i < 4; ++i) {
        ihdr_crc_convert_pointer[3-i] = ihdr_crc_convert >> ((3 - i) * 8);
    }

    U8* gp_buf_inf_sum = malloc((size1 + size2)* sizeof(U8));

    for (int i = 0; i < size1 + size2; i++) {
        if (i < size1) {
            gp_buf_inf_sum[i] = gp_buf_inf1[i];
        } else {
            gp_buf_inf_sum[i] = gp_buf_inf2[i - size1];
        }
    }

    U8* gp_buf_def = malloc((size1 + size2)* sizeof(U8));

    U64 len_def = size1 + size2;

    mem_def(gp_buf_def, &len_def, gp_buf_inf_sum, size1 + size2, Z_DEFAULT_COMPRESSION);

    unsigned long idat_length_convert = ntohl(len_def);

    U8 idat_length_convert_pointer [4];

    for (int i = 0; i < 4; ++i) {
        idat_length_convert_pointer[3-i] = idat_length_convert >> ((3 - i) * 8);
    }

    U8* idat_buffer_without_length = malloc((len_def + 4)* sizeof(U8));

    for (int i = 0; i < len_def + 4; i++) {
        if (i < 4) {
            idat_buffer_without_length[i] = type_buffer[i];
        } else {
            idat_buffer_without_length[i] = gp_buf_def[i - 4];
        }
    }

    unsigned long idat_crc = crc(idat_buffer_without_length,len_def + 4);

    unsigned long idat_crc_convert = ntohl(idat_crc);

    U8 idat_crc_convert_pointer [4];

    for (int i = 0; i < 4; ++i) {
        idat_crc_convert_pointer[3-i] = idat_crc_convert >> ((3 - i) * 8);
    }

    FILE *fp_png;
    fp_png = fopen("all.png", "w");
    fwrite(magic_number,sizeof(magic_number[0]),8,fp_png);
    fwrite(ihdr_buffer,sizeof(ihdr_buffer[0]),21,fp_png);
    fwrite(ihdr_crc_convert_pointer,sizeof(ihdr_crc_convert_pointer[0]),4,fp_png);
    fwrite(idat_length_convert_pointer,sizeof(idat_length_convert_pointer[0]),4,fp_png);
    fwrite(idat_buffer_without_length,sizeof(idat_buffer_without_length[0]),len_def + 4,fp_png);
    fwrite(idat_crc_convert_pointer,sizeof(idat_crc_convert_pointer[0]),4,fp_png);
    fwrite(iend_buffer,sizeof(iend_buffer[0]),12,fp_png);
    fclose(fp_png);
    if(i == 0) {
        remove ("0.png");
    }
    remove (number);

    free(idat_buffer_data1);
    free(gp_buf_inf1);
    free(idat_buffer_data2);
    free(gp_buf_inf2);
    free(gp_buf_inf_sum);
    free(gp_buf_def);
    free(idat_buffer_without_length);
    free(type_buffer);
    }
}

int if_have_all(int arr[])
{
    int i;
    for(i = 0; i < 50; ++i)
    {
        if(arr[i] == -1)
            return 0;
    }
    return 1;
}

struct thread_args              /* thread input parameters struct */
{
    int* array;
    char* url;
};

struct thread_ret               /* thread return values struct   */
{

};

void *do_work(void *arg);  /* a routine that can run as a thread by pthreads */

void *do_work(void *arg)
{
    struct thread_args *p_in = arg;
    struct thread_ret *p_out = malloc(sizeof(struct thread_ret));

    CURL *curl_handle;
    CURLcode res;
    char url[256];
    RECV_BUF recv_buf;
    char fname[256];
    while(if_have_all(p_in->array) == 0) {

    recv_buf_init(&recv_buf, BUF_SIZE);
    
    strcpy(url, p_in->url); 

    curl_global_init(CURL_GLOBAL_DEFAULT);


    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    res = curl_easy_perform(curl_handle);

    if( res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }
    sprintf(fname, "./%d.png", recv_buf.seq);
    p_in->array[recv_buf.seq] = recv_buf.seq;
    write_file(fname, recv_buf.buf, recv_buf.size);

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    recv_buf_cleanup(&recv_buf);
    }
    
    return ((void *)p_out);
}

int main( int argc, char** argv ) 
{
    int c;
    int n = 1;
    int NUM_THREADS = 1;
    char IMG_URL[46];
    sprintf(IMG_URL, "http://ece252-1.uwaterloo.ca:2520/image?img=1");
    
    while ((c = getopt (argc, argv, "t:n:")) != -1) {
        switch (c) {
        case 't':
            NUM_THREADS = strtoul(optarg, NULL, 10);
            break;
        case 'n':
            n = strtoul(optarg, NULL, 10);
            break;

        }
    }

    int seq_arr[50];

    memset(seq_arr, -1, 50 * sizeof(int));

    pthread_t *p_tids = malloc(sizeof(pthread_t) * NUM_THREADS);
    struct thread_args in_params[NUM_THREADS];
    struct thread_ret *p_results[NUM_THREADS];
     
    for (int i=0; i<NUM_THREADS; i++) {
        in_params[i].array = seq_arr;
        sprintf(IMG_URL, "http://ece252-%d.uwaterloo.ca:2520/image?img=%d", i % 3 + 1 , n);
        in_params[i].url = IMG_URL;
        pthread_create(p_tids + i, NULL, do_work, in_params + i); 
    }

    for (int i=0; i<NUM_THREADS; i++) {
        pthread_join(p_tids[i], (void **)&(p_results[i]));
    }

    free(p_tids);
    
    for (int i=0; i<NUM_THREADS; i++) {
        free(p_results[i]);
    }

    catpng();

    return 0;
}
