#include "server.h"

int writing = 0;
static timeval start;
static timeval complete;
static int count = 0;

void init_global() 
{
	int i;
	for(i = 0; i < FRAME_COUNT; ++i) {
		frame_buffer[i].length = 0;
		frame_buffer[i].data = NULL;
		frame_buffer[i].data =  (unsigned char *)malloc(FRAME_BUFFER_SIZE*sizeof(unsigned char));
		if(!frame_buffer[i].data) {
			err_sys("malloc frame buffers");
		}
		memset(frame_buffer[i].data, 0, FRAME_BUFFER_SIZE);
	}
}

void exit_global()
{
	int i;
	for(i = 0; i < FRAME_COUNT; ++i) {
		frame_buffer[i].length = 0;
		free(frame_buffer[i].data);
	}
}

void pthreadid(const char *threadname)
{
    pthread_t pid = pthread_self();
	DBG("%s: %u\n",threadname, (unsigned)pid);
}

/******************************************************************
 *
 * these functions are used to printf error message and exit.
 *
 * ****************************************************************/

void err_doit(int err_no, const char *fmt, va_list list)
{
	int err_no_save;
	char buf[MAXLINE] = {0};

	err_no_save = err_no;
	vsprintf(buf, fmt, list);
	if(err_no_save) {
		sprintf(buf + strlen(buf), ": %s", strerror(err_no_save));
	}

	strcat(buf, "\n");
	fflush(stdout);        // here i do not know why doing this.
	fputs(buf, stderr);
	fflush(NULL);         // flushes all stdio output streamers.
	return;
}

void err_msg(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	err_doit(0, fmt, list);
	va_end(list);
	return;
}

// Fatal error related to a system call.
void err_sys(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	err_doit(1, fmt, list);
	va_end(list);
	exit(1);
}

void err_quit(const char *fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	err_doit(0, fmt, list);
	va_end(list);
	exit(1);
}

/******************************************************************
 *
 * These functions are used to server operations.
 *
 * ****************************************************************/
static void copytobuffer(struct bufferevent *bev, char *buf, frame one)
{
	struct evbuffer_iovec v[2];
	int n = 0;
	int sum_length = strlen(buf) + one.length;
	n = evbuffer_reserve_space(buf, FRAME_BUFFER_SIZE, v, 2);
	for(i = 0; i < n; ++i) {
		// Fix me.
		if(v[i].iov_len < sum_length)
			err_sys("iov_len error");
		memcpy(v[i].iov_base, buf, strlen(buf));
		memcpy(v[i].iov_base + strlen(buf), one.data, one.length);
		iov[i].iov_len = sum_length;
	}
}

static void send_frame_header(struct bufferevent *bev)
{
	char *buf[SEND_BUFFER_SIZE] = {0};
	count = 0;
	
	// mjpeg stream header.
	sprintf(buffer, "HTTP/1.0 200 OK\r\n" \
		"Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
		"\r\n" \
		"--" BOUNDARY "\r\n");

	pthread_mutex_lock(&lock_buf);



}

static void send_frame()
{
}

static void accept_error_cb(struct evconnlistener *listener, void *ctx)
{
	struct event_base *base = evconnlistener_get_base(listener);
	int err = EVUTIL_SOCKET_ERROR();
	err_msg("socket accept error %d(%s)", err, evutil_socket_error_to_string(err));
	event_base_loopexit(base, NULL);
}

static void read_cb(struct bufferevent *bev, void *ctx)
{
	char *request_line;
	size_t len;
	evutil_socket_t fd = bufferevent_getfd(bev);
	struct evbuffer *input = bufferevent_get_input(bev);
	struct evbuffer *output = bufferevent_get_output(bev);
	request_line = evbuffer_readln(input, &len, EVBUFFER_FLAG_DRAINS_TO_FD);
	if(request_line) {
		DBG("line:%s\n", request_line);
		if(strstr(request_line, "GET /stream") != NULL) {
			writing = 1;
		}
			
	}



//	evbuffer_add_buffer(output, input);
	pthreadid("read");
	free(request_line);
	DBG("read, fd:%d\n", fd);
//	char test[] = "hello world";
//	DBG("size:%d\n", sizeof(test));
	//evbuffer_add(output, test, sizeof(test));
//	sleep(5);
//	bufferevent_free(bev);
//	event_base_loopexit(bufferevent_get_base(bev), NULL);
}

static void write_cb(struct bufferevent *bev, void *ctx)
{
	evutil_socket_t fd = bufferevent_getfd(bev);
	if(writing) {
		DBG("write ready, fd:%d\n", fd);
		//bufferevent_free(bev);
		writing = 0;
	}
	else
		DBG("init\n");
	
	pthreadid("write");
}

static void event_cb(struct bufferevent *bev, short events, void *ctx)
{
	if(events & BEV_EVENT_ERROR)
		perror("error from buffer event");
	if(events & (BEV_EVENT_EOF | BEV_EVENT_ERROR))
		bufferevent_free(bev);
}

static void accept_cb(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *ctx)
{
	//DBG("accept\n");
	struct event_base *base = evconnlistener_get_base(listener);
	struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

	bufferevent_setcb(bev, read_cb, write_cb, event_cb, NULL);
	bufferevent_enable(bev, EV_READ|EV_WRITE);
}


void * accept_thread(void *args)
{	
	struct event_base *base = NULL;
	struct evconnlistener *listener;

	struct addrinfo *aip, *aip2, *aip_in4;
	struct addrinfo hints;
	char portstring[NI_MAXHOST];

	int err;

	struct sockaddr_in in4;
	struct sockaddr_in6 in6;

	pthreadid("accept thread");

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	base = event_base_new();
	if(!base) {
		err_sys("event_base_new");
	}

	snprintf(portstring, sizeof(portstring), "%d", PORT);
	if(err = getaddrinfo(NULL, portstring, &hints, &aip)) {
		perror("getaddrinfo:");
		return NULL;
	}
	
	for(aip2 = aip; aip2 != NULL; aip2 = aip2->ai_next) {
		// here we only use the ipv4 address, to complete the ipv6 support in the future.
		if(aip2->ai_family == AF_INET) {
			aip_in4 = aip2;
			break;
		}
	}
	
	listener = evconnlistener_new_bind(base, accept_cb, NULL, LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1, aip_in4->ai_addr, sizeof(*(aip_in4->ai_addr)));
	if(!listener) {
		err_sys("evconnlistener_new_bind");
		return NULL;
	}

	evconnlistener_set_error_cb(listener, accept_error_cb);

	event_base_dispatch(base);
	if(base)
		event_base_free(base);
}










	

