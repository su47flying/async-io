#ifndef __ASYNC_IO_H__
#define __ASYNC_IO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>		/* memset() */
#include <inttypes.h>	/* uint64_t */
#include <sys/types.h>
#include <sys/syscall.h>	/* for __NR_* definitions */
#include <unistd.h>		/* for syscall() */
#include <linux/aio_abi.h>	/* for AIO types and constants */
#include <pthread.h>
#include <syslog.h>
#include <time.h>

#define AIO_NAME_LEN   256

struct async_io_ctx {
	int32_t fd;                  // file description of targe file.
	aio_context_t ctx;
    int32_t stop_event_flag;
	int32_t finished_event_flag;
    u_int16_t io_depth;           // max io depth for setting 
    u_int16_t submit_depth;
	int32_t finished_callback_flag;
    //u_int64_t event_count;
    //u_int64_t write_total;
    //u_int64_t read_total;
    //u_int64_t total_nbytes;
    pthread_mutex_t submit_lock;     //
    pthread_cond_t  submit_wait;
	unsigned char file_name[AIO_NAME_LEN];
};

typedef void (*io_complete_fn) (char *buffer, int32_t, int32_t, u_int64_t);

struct async_io_fops {
	struct async_io_ctx *(*open)(const char *name, int io_depth);
	int   (*read)(struct async_io_ctx *, io_complete_fn, char *, u_int64_t, u_int32_t);
	int   (*write)(struct async_io_ctx *, io_complete_fn, const char *, u_int64_t, u_int32_t len);
	void  (*close)(struct async_io_ctx *);
};

struct aio_iocb_t {
	struct iocb iocb;
	io_complete_fn io_complete;	
};

extern int aio_debug_output ;// 0; syslog 1:console
extern int aio_debug_level ;

#define AIO_DEBUG_TAG      "DEBUG"
#define AIO_INFO_TAG       "INFO"
#define AIO_NOTICE_TAG     "NOTICE"
#define AIO_WARNING_TAG    "WARNING"
#define AIO_ERROR_TAG      "ERROR"

void async_io_init(void);

#define AIOX_DEBUG(p, t, a, x...) \
	do { \
		if (p <= aio_debug_level)                                      \
		{                                                              \
            if(aio_debug_output == 1) {                                \
                printf("[%s]%s:%d: async-io: "a, t, __func__, __LINE__, ##x);          \
				printf("\n"); \
		} else {                                                                   \
                syslog(p, "[%s] %s:%s:%d: async-io: "a, t, __FILE__, __func__, __LINE__, ##x );        \
            }                                                                          \
        } \
    } while (0)

#define AIO_LOG_ERROR(fmt, x...)             AIOX_DEBUG(LOG_ERR, AIO_ERROR_TAG, fmt,  ##x )
#define AIO_LOG_WARNING(fmt, x...)           AIOX_DEBUG(LOG_WARNING, AIO_WARNING_TAG, fmt, ##x)
#define AIO_LOG_NOTICE(fmt, x...)            AIOX_DEBUG(LOG_NOTICE, AIO_NOTICE_TAG, fmt, ##x )
#define AIO_LOG_INFO(fmt, x...)              AIOX_DEBUG(LOG_INFO, AIO_INFO_TAG, fmt, ##x )
#define AIO_LOG_DEBUG(fmt, x...)             AIOX_DEBUG(LOG_DEBUG, AIO_DEBUG_TAG, fmt, ##x )

static inline void io_prep_pread(struct iocb *iocb, int fd, void *buf, int nr_segs,
		      int64_t offset)
{
	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = fd;
	iocb->aio_lio_opcode = IOCB_CMD_PREAD;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (u_int64_t) buf;
	iocb->aio_nbytes = nr_segs;
	iocb->aio_offset = offset;
	//iocb->aio_flags = IOCB_FLAG_RESFD;
	//iocb->aio_resfd = afd;
}

static inline void io_prep_pwrite(struct iocb *iocb, int fd, void const *buf, int nr_segs,
	int64_t offset)
{
	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = fd;
	iocb->aio_lio_opcode = IOCB_CMD_PWRITE;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (u_int64_t) buf;
	iocb->aio_nbytes = nr_segs;
	iocb->aio_offset = offset;
	//iocb->aio_flags = IOCB_FLAG_RESFD;
	//iocb->aio_resfd = afd;
}

static inline void io_event_setup(struct iocb *iocb, int afd)
{
       iocb->aio_flags = IOCB_FLAG_RESFD;
       iocb->aio_resfd = afd;
}

static inline int io_setup(unsigned nr, aio_context_t *ctxp)
{
	return syscall(__NR_io_setup, nr, ctxp);
}
  
static inline int io_destroy(aio_context_t ctx) 
{
	return syscall(__NR_io_destroy, ctx);
}

static inline int io_submit(aio_context_t ctx, long nr,  struct iocb **iocbpp) 
{
	return syscall(__NR_io_submit, ctx, nr, iocbpp);
}
 
static inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
	struct io_event *events, struct timespec *timeout)
{
	return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}

struct async_io_fops aio_fops;
     

#endif /* __ASYNC_IO_H__ */
