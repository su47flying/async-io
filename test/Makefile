CC=gcc --std=gnu99

INCLUDES := -I ../include 

#SRCS := $(wildcard lib/*.c)
SRCS := verify-aio-test.c 
LIBS := -lpthread -lrt -lasync-io
TARGET := verify-aio.out
LIB_PATH := -L../

OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

current_dir = $(shell pwd)
#FLAG= -g $(INCLUDES)
CFLAGS = -g
LDFLAGS := -Wl,-R -Wl,$(current_dir)/../
#LDFLAGS :=

.PHONY:all
all:$(OBJS)	
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) $(LIB_PATH) $(LIBS)
	@echo $(TARGET) has been built successfully!

-include $(DEPS) 

objs:$(OBJS)
	@echo Compile protocol objects finished.

%.o:%.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -c $< -o $@
	
%.d:%.c
	@set -e; rm -f $@; \
	$(CC) $(INCLUDES) $< -MM -MT $(@:.d=.o) >$@
#$(CC) -MM $(INCLUDES) $< > $@.$$$$; \
#sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
#rm -f $@.$$$$

.PHONY:clean
clean:
	rm -f $(OBJS) $(DEPS)
	rm -f $(TARGET)
