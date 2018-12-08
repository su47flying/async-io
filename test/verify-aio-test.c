
#define _GNU_SOURCE
#define __STDC_FORMAT_MACROS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "async-io.h"
void async_io_init(void);

/* generate source data, for wrinting into disk/file. */
static char *generate_data(u_int64_t offset, int len)
{
	char *buffer = NULL;
	/* buffer must align by 512, 
	*  otherwise,  error code (-22)  be return from callback function's argument ret . 
	*/
	posix_memalign((void**)(&buffer),512,len);
	for(int i = 0; i < len; i += sizeof(u_int64_t)) {		
		memcpy(buffer + i, (char *)&offset, sizeof(u_int64_t));
		offset += sizeof(u_int64_t);
	}

	return buffer;
}

/* generate a empty buffer for reading */
static char *generate_buffer(u_int32_t len) 
{
	char *buffer = NULL;
	posix_memalign((void**)(&buffer),512,len);
	return buffer;
}

static int verify_data(char *buffer, int len, u_int64_t offset)
{
	int ret = 0;
	for(int i = 0; i < len; i += sizeof(u_int64_t)) {
		if(*((u_int64_t *)(buffer + i)) != offset) {
			AIO_LOG_INFO("data verify error. offset:%lu actual:0x%lx expected:0x%lx", 
				          offset, *((u_int64_t *)(buffer + i)), offset);
			//printf("buffer:0x%lx offset:0x%lx\n", *((u_int64_t *)(buffer + i)), offset);
		    ret = -1;
			assert(0);
		}
		AIO_LOG_INFO("data verify correctly. offset:%lu actual:0x%lx expected:0x%lx", 
				          offset, *((u_int64_t *)(buffer + i)), offset);
		offset += sizeof(u_int64_t);
	}

	return ret;
}


void io_callback_write(char *buffer, int res, int32_t nbytes, u_int64_t offset)
{
	if(res != nbytes) {
		AIO_LOG_ERROR("aio write callback error. offset:%lu res:%d nbytes:%d",
			           offset, res, nbytes);
	}

	free(buffer);
}

void io_callback_read(char *buffer, int res, int32_t nbytes, u_int64_t offset)
{
	if(res != nbytes) {
		AIO_LOG_ERROR("read error. offset:%lu res:%d", offset, res);
		assert(res == nbytes);
	}
	verify_data(buffer, res, offset);

	free(buffer);
}

/*  write data inot file */
void aio_write(const char *file_name, int count, int block_size)
{
	struct async_io_ctx *aio_ctx = aio_fops.open(file_name, 32);
	if(!aio_ctx) {
		AIO_LOG_ERROR("aio open erro.");
		return;
	}
	u_int64_t offset = 0;
	for(int i = 0; i < count; i++) {
		char *buffer = generate_data(offset, block_size) ;		
		int ret = aio_fops.write(aio_ctx, io_callback_write, buffer, offset, block_size);
		if(ret != 0){
			AIO_LOG_ERROR("write error. offset:%lu", offset);
		}
		offset += block_size;	
	}

	aio_fops.close(aio_ctx);
}

/* read data from file */
void aio_read(const char *file_name, int count, int block_size)
{
	struct async_io_ctx *aio_ctx = aio_fops.open(file_name, 32);
	if(!aio_ctx) {
		//AIO_LOG_ERROR("aio open erro.");
		return;
	}
	u_int64_t offset = 0;
	for(int i = 0; i < count; i++) {
		char *buffer = generate_buffer(block_size) ;
		int ret = aio_fops.read(aio_ctx, io_callback_read, buffer, offset, block_size);
		if(ret != 0){
			AIO_LOG_ERROR("read error. offset:%lu", offset);
		}
		offset += block_size;	
	}

	aio_fops.close(aio_ctx);

}

int main(int argc, char *argv[])
{	
	 int aio_debug_output = 1 ;// 0; syslog 1:console
	 int aio_debug_level = LOG_INFO ;

	if(argc != 3) {
		AIO_LOG_INFO("args too short:%d...", argc);
		exit(-1);
	}

	char *file_name = argv[1];
	int count = atoi(argv[2]);

	AIO_LOG_INFO("file name:%s count:%d", file_name, count);

	async_io_init();
	/* first write data into file */
	aio_write(file_name, count, 8192);
	/* read data from file and verify it. */
	aio_read(file_name, count, 8192);
	
}
