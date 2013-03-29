# Makefile write by sxm.
# 2013.3.28

CC := gcc
CFLAGS += -g
INCLUDE := -I/opt/libjpeg-turbo/include/
LDFLAGS += -L/opt/libjpeg-turbo/lib64
OBJECTS := main.o server.o compress.o
LIBS = -ljpeg -lpthread -levent
COMPILE = $(CC) $(CFLAGS) $(INCLUDE) $(LDFLAGS) $(LIBS)

streamer: $(OBJECTS)
	$(COMPILE) -o streamer $(OBJECTS)

$(OBJECTS): %.o:%.c server.h
	$(COMPILE) -c $<


#$(CC) $(CFLAGS) $(INCLUDE) $(LDFLAGS) $(LIBS) -c $< 

.PHONY: clean
clean:
	-rm  -f streamer *.o
