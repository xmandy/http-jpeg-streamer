#ifndef SERVER_H
#define SERVER_H

#define DEBUG

#define IO_BUFFER_SIZE 256
#define SEND_BUFFER_SIZE 1024
#define MAX_SD_LEN 8

#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define MAX(a, b) (((a) < (b)) ? (b) : (a))

#define STD_HEADER "Connection: keep-alive\r\n"\
		"Server: MJPEG-Streamer/demo\r\n"\
		"Cache-control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n"\
		"Pragma:no-cache\r\n"\
		"Expires: Tue, 26 Feb 2013 18:00:00 GMT\r\n"

#ifdef DEBUG
#define DBG(...) fprintf(stderr, "DBG(%s, %s(), %d): ", __FILE__, __FUNCTION__, __LINE__), fprintf(stderr, __VA_ARGS__)
#else
#define DBG(...)
#endif

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
		

#endif
