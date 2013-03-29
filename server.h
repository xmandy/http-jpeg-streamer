#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <arpa/inet.h>
#include <netdb.h> // NI_MAXHOST, addr info.
#include <pthread.h>
#include <signal.h>

#include "jpeglib.h"

#define DEBUG

#define IO_BUFFER_SIZE 256
#define PORT 8080
#define MAXLINE 1024
#define SEND_BUFFER_SIZE 1024
#define MAX_SD_LEN 8
#define FRAME_BUFFER_SIZE 500000
#define FRAME_COUNT 50

#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))

#define STD_HEADER "Connection: close\r\n"\
		"Server: MJPEG-Streamer/demo\r\n"\
		"Cache-control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n"\
		"Pragma:no-cache\r\n"\
		"Expires: Tue, 26 Feb 2013 18:00:00 GMT\r\n"

#define BOUNDARY "boundarydonotcross"

#ifdef DEBUG
#define DBG(...) fprintf(stderr, "DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__), fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif

/****************************************************
 *
 * struct definition.
 *
 * *************************************************/

typedef struct {
		int level; // how full is the buff
		char buffer[IO_BUFFER_SIZE];
} iobuffer;

typedef enum {
	A_UNKNOWN,
	A_SNAPSHOT,
	A_STREAM,
	A_COMMAND,
	A_FILE,
	A_INPUT_JSON,
	A_OUTPUT_JSON,
	A_PROGRAM_JSON,
} answer_t;

typedef struct {
	answer_t type;
	char *parameter;
	char *client;
	char *credentials;
} request;

// The structures are for frame buffer.
typedef struct {
	int length;
	unsigned char *data;
} frame;

/******************************************************************
 *
 * Global variables statement.
 *
 * ****************************************************************/

extern frame frame_buffer[FRAME_COUNT];
extern struct bufferevent *bufev;
extern int writing;
extern int frame_header;
extern pthread_mutex_t lock_write;
extern pthread_mutex_t lock_buf;

/******************************************************************
 *
 * these functions are used to printf error message and exit.
 *
 * ****************************************************************/

extern void err_doit(int err_no, const char *fmt, va_list list);
extern void err_msg(const char *fmt, ...);
extern void err_sys(const char *fmt, ...);
extern void err_quit(const char *fmt, ...);

/******************************************************************
 *
 * these functions and macros are used for compressing images.
 *
 * ****************************************************************/
extern int read_byte(FILE *input_file);
extern unsigned char* get_24bit_row(unsigned char *imagedata, long row_number, long height, long row_width, long width);
extern unsigned char* process_bmpfile(FILE *input_file, long *height, long *width);
#define GET_2B(array, offset) ((unsigned int) ((unsigned char)(array[offset])) + \
								((unsigned int) ((unsigned char)(array[offset+1])) << 8))
#define GET_4B(array, offset) ((long) ((unsigned char)(array[offset])) + \
								(((long) ((unsigned char)(array[offset+1]))) << 8) + \
								(((long) ((unsigned char)(array[offset+2]))) << 16) + \
								(((long) ((unsigned char)(array[offset+3]))) << 24)) 

/****************************************************************
 *
 * These function are common used function in the program, see server.c.
 *
 * *************************************************************/

extern void init_global();
extern void exit_global();
extern void * accept_thread(void*);
extern void read_cb(struct bufferevent *bev, void *ctx);
extern void write_cb(struct bufferevent *bev, void *ctx);
extern void event_cb(struct bufferevent *bev, short events, void *ctx);
extern void pthreadid(const char *threadname);

#endif
