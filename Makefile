CC=gcc

INCLUDES := -I ./include 
SRCS := $(wildcard src/*.c)
LIBS := 
TARGET := libasync-io.so
LIB_PATH :=  -lpthread -lrt

OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)

#FLAG= -g $(INCLUDES)
CFLAGS = -g -fPIC -shared
#LDFLAGS :=
.PHONY: all 
all:$(OBJS)	
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LIB_PATH) $(LIBS)
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

.PHONY: clean
clean:
	rm -f $(OBJS) $(DEPS)
	rm -f $(TARGET)
