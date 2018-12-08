
#define _GNU_SOURCE
#define __STDC_FORMAT_MACROS

#include <errno.h>
#include <fcntl.h> /* O_RDWR */
#include <pthread.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <syslog.h>
#include <time.h>

#include "async-io.h"

int aio_debug_output = 1 ;// 0; syslog 1:console
int aio_debug_level = LOG_INFO ;

#define AIO_NR  1024  /* io_setup limit */
#define AIO_MAX_NR_GETEVENTS 8

typedef enum _AIO_OPS_T{
	AIO_OPS_READ = 1,   /* read */
	AIO_OPS_WRITE = 2,  /* write */
	MAX_AIO_OPS
}AIO_OPS_T;

void *asyn_io_getevents_thread(void *data);

struct async_io_ctx * async_io_open(const char *file_name, int io_depth)
{
	AIO_LOG_INFO("open async io file name:%s io depth:%d", file_name, io_depth);
	int ret = -1;
	struct async_io_ctx *aio_ctx = NULL;
	aio_ctx = calloc(1, sizeof(struct async_io_ctx)); 
	if(NULL == aio_ctx) {
		AIO_LOG_ERROR("calloc memory erro. %d:%s", errno, strerror(errno));
		return NULL;
	}
	aio_ctx->fd = -1;
	aio_ctx->io_depth = io_depth;
	strncpy(aio_ctx->file_name, file_name, sizeof(aio_ctx->file_name) - 1);

	ret = pthread_mutex_init(&aio_ctx->submit_lock, NULL);
    if (ret != 0) {
        AIO_LOG_ERROR("pthread_mutex_init iocb lock error. %d:%s", errno,
                     strerror(errno));
        goto err;
    }

    ret = pthread_cond_init(&aio_ctx->submit_wait, NULL);
    if (ret != 0) {
        AIO_LOG_ERROR("pthread_cond_init submit wait error. %d:%s", errno,
                     strerror(errno));
        goto err;
    }
    
	AIO_LOG_NOTICE("open: file:%s io depth:%d", aio_ctx->file_name, io_depth);
	
    int32_t fd = open(aio_ctx->file_name, O_RDWR | O_CREAT | O_DIRECT, 0644);
    if (fd == -1) {
        AIO_LOG_ERROR("open file:%s error. %d:%s", aio_ctx->file_name, errno,
                     strerror(errno));
        goto err;
    }

    aio_ctx->fd = fd;

	ret = io_setup(AIO_NR, &aio_ctx->ctx);
    if (ret == -1) {
        goto err;
    }

	pthread_t td;

	ret = pthread_create(&td, NULL, asyn_io_getevents_thread, (void *)aio_ctx );
	if(ret < 0) {
		AIO_LOG_ERROR("create thread error.");
		goto err;
	}
	pthread_detach(td);
	/* open completely */
	AIO_LOG_INFO("aio open completely. file name:%s fd:%d ctx:%lu", 
				  aio_ctx->file_name, aio_ctx->fd, aio_ctx->ctx);
	return aio_ctx;
err:
	
	if(aio_ctx != NULL) {
		if(aio_ctx->fd > 0) 
			close(aio_ctx->fd);

		free(aio_ctx);
		aio_ctx = NULL;
	}
	return NULL;
}

/**
* blocking, when io depth over setting. 
* if await_sec more than zero,  wait eather timeout or io depth less than setting.
* if awati_sec equal zero, wati until io depth less than setting.
*/
static int inc_iodepth(struct async_io_ctx *aio_ctx, int wait_sec) {
    if (aio_ctx == NULL) {
        AIO_LOG_ERROR("aio_ctx was NULL.....\n");
        return -1;
    }

    int32_t io_depth = aio_ctx->io_depth;
    if (pthread_mutex_lock(&aio_ctx->submit_lock) != 0) {
        AIO_LOG_ERROR("lock error. \n");
        return -1;
    }

    struct timespec ts;
    while (aio_ctx->submit_depth >= io_depth) {
		if(wait_sec > 0) {
	        /* wait for timeout */
	        clock_gettime(CLOCK_REALTIME, &ts);
	        // ts.tv_nsec = 0;
	        ts.tv_sec += wait_sec;
	        int ret = pthread_cond_timedwait(&aio_ctx->submit_wait,
	                                         &aio_ctx->submit_lock, &ts);
	        if (ret == ETIMEDOUT) {
	            AIO_LOG_ERROR(
	                "---NOTICE----- wait over one second. submit_count:%u io "
	                "depth:%d storage:%s",
	                aio_ctx->submit_depth, io_depth, aio_ctx->file_name);
	        }
        } else {
	        /* wait for no timeout */
	        int ret = pthread_cond_wait(&aio_ctx->submit_wait, &aio_ctx->submit_lock);
	        if(ret != 0) {
	            AIO_LOG_ERROR("-----NOTICT----- wait error:%s(%d)", strerror(errno), errno);
	        }
		}
    }

    // TODO shoud use __sync_add_and_fetch()
    aio_ctx->submit_depth++;

    pthread_mutex_unlock(&aio_ctx->submit_lock);

    return 0;
}

/* decrea the count from submit depth, when return the io result from the io_getevents. */
static int dec_iodepth(struct async_io_ctx *aio_ctx, int count) {
    if (aio_ctx == NULL) {
        AIO_LOG_ERROR("aio_ctx was NULL.....\n");
        return -1;
    }

    int32_t io_depth = aio_ctx->io_depth;
    if (pthread_mutex_lock(&aio_ctx->submit_lock) != 0) {
        AIO_LOG_ERROR("lock error.\n");
        return -1;
    }

    if (aio_ctx->submit_depth == io_depth) {
    /* you must use broadcast, becaust count may be more than one. */
        //pthread_cond_signal(&aio_ctx->submit_wait);
        pthread_cond_broadcast(&aio_ctx->submit_wait);
    }

    // TODO shoud use __sync_sub_and_fetch()
    aio_ctx->submit_depth -= count;

    pthread_mutex_unlock(&aio_ctx->submit_lock);

    return 0;
}

int async_io_ops(struct async_io_ctx *aio_ctx, io_complete_fn io_complete, const char *buf, u_int64_t offset, u_int32_t size, int ops)
{
	int ret = -1;
	struct iocb *iocbp = NULL;
	struct aio_iocb_t *aio_iocb = NULL;
	if(inc_iodepth(aio_ctx, 0) != 0) {
		AIO_LOG_ERROR("incream io depth erro.");
		return -1;
	}

	aio_iocb = calloc(1, sizeof(struct aio_iocb_t));
	if(aio_iocb == NULL) {
		AIO_LOG_ERROR("calloc error!!!.");		
		goto err;	
	}
	
	aio_iocb->io_complete = io_complete;
	iocbp = &aio_iocb->iocb;
	if(ops == AIO_OPS_READ)
		io_prep_pread(iocbp, aio_ctx->fd, (void *)buf, size, offset);
	else if(ops == AIO_OPS_WRITE) 
		io_prep_pwrite(iocbp, aio_ctx->fd, buf, size, offset);
	else {
		AIO_LOG_ERROR("unknown ops:%d", ops);
		goto err;
	}

	/* check if has set stot flag */
	if(aio_ctx->stop_event_flag) {
		AIO_LOG_INFO("stop flag had bee set. so don't do io submit");
		ret = -1;
		goto err;
	}
	
	iocbp->aio_data = (u_int64_t)aio_iocb;
	int rc = io_submit(aio_ctx->ctx, 1, &iocbp);
	if (rc == 1) {
		ret = 0;
		goto out;
	} else {
		ret = rc;
		AIO_LOG_ERROR("io submit error:%d size:%u offset:%lu op:%d\n",
			           rc, size, offset, ops);
		goto err;
	}
	
err:
	dec_iodepth(aio_ctx, 1);
	if(aio_iocb != NULL) {
		free(aio_iocb);
		aio_iocb = NULL;
	}
out:
	AIO_LOG_INFO("aio submit completely(%d). ops:%d offset%lu, size:%u", 
		          ret, ops, offset, size);
	return ret;

}

int async_io_read(struct async_io_ctx *ctx, io_complete_fn io_complete, char *buf, u_int64_t offset, u_int32_t size)
{
	return async_io_ops(ctx, io_complete, buf, offset, size, AIO_OPS_READ);
}

int async_io_write(struct async_io_ctx *ctx, io_complete_fn io_complete, const char * buf, u_int64_t offset, u_int32_t size)
{
	return async_io_ops(ctx, io_complete, buf, offset, size, AIO_OPS_WRITE);
}

/* get io events and invoke io callback. */
void *asyn_io_getevents_thread(void *data)
{	
	int ret;
	struct async_io_ctx *aio_ctx = (struct async_io_ctx *)data;
	struct io_event    *event = NULL;
    struct aio_iocb_t  *aio_iocb = NULL;
    struct io_event     events[AIO_MAX_NR_GETEVENTS];
    struct timespec     ts;    
	bzero(&ts, sizeof(ts));
    ts.tv_sec = 1;
	char thread_name[128];
	
	bzero(thread_name, sizeof(thread_name));
    snprintf(thread_name, sizeof(thread_name) - 1, "aio-event-handler-%s-%d",
             aio_ctx->file_name, aio_ctx->fd);
    prctl(PR_SET_NAME, thread_name);
    AIO_LOG_NOTICE("thread:%s start...", thread_name);
    while(1){
		memset (&events[0], 0, sizeof (events));
        ret = io_getevents (aio_ctx->ctx, 1, AIO_MAX_NR_GETEVENTS,
                            &events[0], &ts);
		
		if(ret > 0) {
			dec_iodepth(aio_ctx, ret);
	        for(int i = 0; i < ret ; i++) {
	        
				event = &events[i];
				aio_iocb = (struct aio_iocb_t *)event->data;
				AIO_LOG_INFO("get io event. offset:%lld res:%lld", aio_iocb->iocb.aio_offset, event->res);
				if(aio_iocb->io_complete != NULL) {
					aio_iocb->io_complete((char *)aio_iocb->iocb.aio_buf, event->res, aio_iocb->iocb.aio_nbytes, aio_iocb->iocb.aio_offset);
				}
	        }
        } else if (ret < 0) {
			AIO_LOG_ERROR("%s: io_getevents() returned %d, exiting", thread_name, ret);
			if (ret == -EINTR) {
				continue;
			}
			break;			
		} else if(ret == 0) {
			AIO_LOG_INFO("aio get event timeout");
		    /* timeout */
			if(aio_ctx->stop_event_flag == 1 && aio_ctx->submit_depth == 0) {
				//TODO atomic			
				break;
			}
		}
    }
    aio_ctx->finished_event_flag = 1;
    return NULL;
}

/* close file descriptor and release resource. */
void async_io_release(struct async_io_ctx *aio_ctx)
{
	AIO_LOG_NOTICE("release async io resource:%s", aio_ctx->file_name);
	pthread_mutex_destroy(&aio_ctx->submit_lock);
	pthread_cond_destroy(&aio_ctx->submit_wait);

	if(aio_ctx->ctx > 0) {
		AIO_LOG_NOTICE("io destory:%s", aio_ctx->file_name);
		io_destroy(aio_ctx->ctx);
	}
	
	if(aio_ctx->fd > 0) {
		AIO_LOG_NOTICE("close file:%s", aio_ctx->file_name);
		close(aio_ctx->fd);
	}
	
	free(aio_ctx);
}

void async_io_close(struct async_io_ctx *aio_ctx)
{
	aio_ctx->stop_event_flag = 1;
	
	int wait_time = 5;
	while (wait_time > 0) {
		if (aio_ctx->finished_event_flag == 1) {
			break;
		}
		sleep(1);

		wait_time--;
	}
	
	if (aio_ctx->finished_event_flag == 1) {
        /* event handle thread finished(exit), so we can free resource */
        AIO_LOG_NOTICE("storage:%s has been closed, resouce  be release",
                     aio_ctx->file_name);
	    async_io_release(aio_ctx);
    } else {
        /* over timeout, but event handle thread don't finished(exit) */
        AIO_LOG_ERROR("storage:%s will been closed, but resouce don't be free",
                     aio_ctx->file_name);
    }
	
}

void async_io_init(void)
{
	aio_fops.open = async_io_open;
	aio_fops.read = async_io_read;
	aio_fops.write = async_io_write;
	aio_fops.close = async_io_close;
}	

