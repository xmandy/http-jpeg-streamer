#include "server.h"

int header_send = 0;
static struct timeval start;
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
		frame_buffer[i].time.tv_sec = 0;
		frame_buffer[i].time.tv_usec = 0;
		pthread_mutex_init(&frame_buffer[i].lock, NULL);
		memset(frame_buffer[i].data, 0, FRAME_BUFFER_SIZE);
	}
}

void exit_global()
{
	int i;
	for(i = 0; i < FRAME_COUNT; ++i) {
		frame_buffer[i].length = 0;
		free(frame_buffer[i].data);
		pthread_mutex_destroy(&frame_buffer[i].lock);
	}
}

int get_elapse(struct timeval start, struct timeval end)
{
	return (end.tv_sec - start.tv_sec)*1000000 + end.tv_usec - start.tv_usec;
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
 ******************************************************************/
static void copytobuffer(struct evbuffer *evbuf, char *buf, frame one, char *boundary)
{
	struct evbuffer_iovec v[2];
	int n = 0;
	int i;
	int sum_length = strlen(buf) + one.length + strlen(boundary);
	n = evbuffer_reserve_space(evbuf, FRAME_BUFFER_SIZE, v, 2);
	for(i = 0; i < n; ++i) {
		// Fix me.
		if(v[i].iov_len < sum_length)
			err_sys("iov_len error");
		memcpy(v[i].iov_base, buf, strlen(buf));
		memcpy(v[i].iov_base + strlen(buf), one.data, one.length);
		memcpy(v[i].iov_base + strlen(buf) + one.length, boundary, strlen(boundary));
		v[i].iov_len = sum_length;
	}
	evbuffer_commit_space(evbuf, v, n);	
}

static void begin_flow_control(int is_header)
{
	int err;
	struct timeval complete;
	int elapse, sleep_usecs, skip_frames;
	int usecs = FRAME_USECS;

	if(is_header) {
		if((err = gettimeofday(&start, NULL)) != 0)
			err_sys("gettimeofday");
		return;
	}

	if((err = gettimeofday(&complete, NULL)) != 0)
			err_sys("gettimeofday");

	elapse = get_elapse(start, complete);	
//	DBG("elapse:%d, %d\n", elapse, elapse/usecs);

	// Refresh the frame start ending time. 
	if((err = gettimeofday(&start, NULL)) != 0)
			err_sys("gettimeofday");

	if(elapse < usecs) {
		usleep(FRAME_USECS - elapse);
		count ++;
		return;
	}
	count += elapse/usecs;
	return;
}

static void send_header(struct bufferevent *bev)
{
	// Here we send the frame whose compressed time is closest to the current time. 
	int err, i;
	int min, min_num, abs, find;
	char header[SEND_BUFFER_SIZE] = {0};
	char buf[SEND_BUFFER_SIZE] = {0};
	char boundary[SEND_BUFFER_SIZE] = {0};
	int n = 0;
	int sum_length; 
	struct timeval current;
	struct evbuffer_iovec v[2];
	struct evbuffer *evbuf; 

	evbuf = bufferevent_get_output(bev);
	find = 0;
	if((err = gettimeofday(&current, NULL)) != 0)
			err_sys("gettimeofday");

	for(i = FRAME_COUNT - 1; i >= 0; --i) {
		pthread_mutex_lock(&(frame_buffer[i].lock));
		abs = fabs(get_elapse(current, frame_buffer[i].time)); 
		if(abs < FRAME_USECS) {
			min_num = i;
			find = 1;
			break;
		}
		if(i == FRAME_COUNT - 1) {
			min = abs;
			min_num = i;
			pthread_mutex_unlock(&(frame_buffer[i].lock));
			continue;
		}
		if(min < abs) {
			min = abs;
			min_num = i;
		}
		pthread_mutex_unlock(&(frame_buffer[i].lock));
	}

	// Here we set the count and send first frame.
	count = min_num;
	DBG("start frame:%d\n", count);
	if(!find)
		pthread_mutex_lock(&(frame_buffer[count].lock));

	begin_flow_control(1);
	DBG("header\n");
	// mjpeg stream header.
	sprintf(header, "HTTP/1.0 200 OK\r\n" \
		"Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n" \
		"\r\n" \
		"--" BOUNDARY "\r\n");
	sprintf(buf, "Content-Type: image/jpeg\r\n" \
	                 "Content-Length: %d\r\n" \
	                 "\r\n", frame_buffer[count].length);
	sprintf(boundary, "\r\n--" BOUNDARY "\r\n");	

	
	sum_length = strlen(header) + strlen(buf) + frame_buffer[count].length + strlen(boundary);
	n = evbuffer_reserve_space(evbuf, FRAME_BUFFER_SIZE, v, 2);
	for(i = 0; i < n; ++i) {
		// Fix me.
		if(v[i].iov_len < sum_length)
			err_sys("iov_len error");
		memcpy(v[i].iov_base, header, strlen(header));
		memcpy(v[i].iov_base + strlen(header), buf, strlen(buf));
		memcpy(v[i].iov_base + strlen(header) + strlen(buf), frame_buffer[count].data, frame_buffer[count].length);
		memcpy(v[i].iov_base + strlen(header) + strlen(buf) + frame_buffer[count].length, boundary, strlen(boundary));
		v[i].iov_len = sum_length;
	}
	evbuffer_commit_space(evbuf, v, n);	
	pthread_mutex_unlock(&(frame_buffer[count].lock));


//	evbuffer_add(bufferevent_get_output(bev), buf, strlen(buf));

}

static void send_frame(struct bufferevent *bev)
{
	begin_flow_control(0);
	char buf[SEND_BUFFER_SIZE] = {0};
	char boundary[SEND_BUFFER_SIZE] = {0};
	count = count % FRAME_COUNT;
	DBG("send frame:%d\n", count);

	sprintf(boundary, "\r\n--" BOUNDARY "\r\n");	



	pthread_mutex_lock(&(frame_buffer[count].lock));
	sprintf(buf, "Content-Type: image/jpeg\r\n" \
	                 "Content-Length: %d\r\n" \
	                 "\r\n", frame_buffer[count].length);
	copytobuffer(bufferevent_get_output(bev), buf, frame_buffer[count], boundary);
	pthread_mutex_unlock(&(frame_buffer[count].lock));
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
			if(header_send)
				bufferevent_free(bev);
			send_header(bev);
			header_send = 1;
		}
	}



//	evbuffer_add_buffer(output, input);
	//pthreadid("read");
	free(request_line);
//	DBG("read, fd:%d\n", fd);
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
	if(header_send) {
		send_frame(bev);
	}
	else
		DBG("init\n");
	
//	pthreadid("write");
}

static void event_cb(struct bufferevent *bev, short events, void *ctx)
{
	DBG("event_cb:%d\n", events);
	if(events & BEV_EVENT_ERROR)
		perror("error from buffer event");
	if(events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
		// The client is closed or other error, init the global area.
		header_send = 0;
		count = 0;
		bufferevent_free(bev);
	}
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

	//pthreadid("accept thread");

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










	

