#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> // NI_MAXHOST, addrinfo defination.
#include <pthread.h>
#include <fcntl.h>
#include "server.h"


void init_request(request *req)
{
	req->type        = A_UNKNOWN;
	req->type        = A_UNKNOWN;
	req->parameter   = NULL;
	req->client      = NULL;
	req->credentials = NULL;
}

void free_request(request *req)
{
	free(req->parameter);
	free(req->client);
	free(req->credentials);
}

void init_iobuffer(iobuffer *iobuf)
{
	memset(iobuf->buffer, 0, sizeof(iobuf->buffer));
	iobuf->level = 0;
}


void send_stream(int fd)
{
	unsigned char *frame = NULL;
		
}

void send_snapshot(int fd)
{
	unsigned char *frame = NULL;
	int frame_size = 0;
	char buffer[SEND_BUFFER_SIZE]={0};
	
	FILE *input;	
	const char *name = "/workspace/netcomponent/jpg/shot-001.jpg";

	int bytes_written, bytes_left;
	char *written_ptr;


	if((input = fopen(name, "rb")) == NULL) {
			DBG("open file failed!\n");
			return;
	}
	
	fseek(input, 0, SEEK_END);
	frame_size = ftell(input);
//	DBG("jpgsize:%d\n", frame_size);

	frame = (unsigned char*) malloc(sizeof(unsigned char)*frame_size);
	
//	DBG("framesize:%d\n", sizeof(frame));


	if(!frame) {
		DBG("malloc for frame buffer failed!\n");
		return;
	}

	fseek(input, 0, SEEK_SET);
	
	if(fread(frame, 1, frame_size, input) != frame_size) {
			DBG("read file failed!\n");
			free(frame);
			fclose(input);
			return;
	}

	sprintf(buffer, "HTTP/1.0 200 OK\r\n"\
			"Content-Type: image/jpeg\r\n"\
			"Content-Length: %d\r\n"\
			STD_HEADER \
			"\r\n", frame_size);
//	DBG("Header:%s\n", buffer);
	if(write(fd, buffer, strlen(buffer)) < 0) {
			free(frame);
			fclose(input);
			DBG("write failed!\n");
			return;
	}
	else {
		written_ptr = frame;
		bytes_left = frame_size;
		bytes_written = 0;	
		while(bytes_left > 0) {
			bytes_written = write(fd, written_ptr, bytes_left);
			DBG("written:%d\n", bytes_written);
			if(bytes_written < 0) {
				DBG("write jpeg data failed!\n");
				free(frame);
				fclose(input);
				return;
			}
			bytes_left -= bytes_written;
			written_ptr += bytes_written;
		}
	}

	fclose(input);
	free(frame);
}

void send_error(int fd, int code, char *message)
{
	char buffer[SEND_BUFFER_SIZE] = {0};
	char error_reason[256];
	int length;
	memset(error_reason, '\0', sizeof(error_reason));
	switch(code) {
	case 400:
			sprintf(error_reason, "400: Not Found!\r\n"\
					"%s\r\n", message);
			length = strlen(error_reason);
			
			sprintf(buffer, "HTTP/1.0 400 Bad Request\r\n" \
					"Content-type: text/plain\r\n" \
					"Content-Length: %d\r\n"\
					STD_HEADER \
					"\r\n" \
					"%s", length, error_reason);
			break;
	default:
			break;
	}

	fprintf(stderr, "%s", buffer);

	if(write(fd, buffer, strlen(buffer)) < 0) {
		   fprintf(stderr, "write failed!\n");
	}	   

			   	
}

int hex_char_to_int(char in)
{
	if(in >= '0' && in <= '9')
			return in - '0';
	if(in >= 'a' && in <= 'f')
			return (in - 'a') + 10;
	if(in >= 'a' && in <= 'f')
			return (in - 'a') + 10;
	return -1;
}

int unescape(char *string)
{
    char *source = string, *destination = string;
	int src, dst, length = strlen(string), rc;

	/* iterate over the string */
	for(dst = 0, src = 0; src < length; src++) {

			/* is it an escape character? */
			if(source[src] != '%') {
					/* no, so just go to the next character */
					destination[dst] = source[src];
					dst++;
					continue;
			}

			/* yes, it is an escaped character */

			/* check if there are enough characters */
			if(src + 2 > length) {
					return -1;
					break;
			}

			/* perform replacement of %## with the corresponding character */
			if((rc = hex_char_to_int(source[src+1])) == -1) return -1;
			destination[dst] = rc * 16;
			if((rc = hex_char_to_int(source[src+2])) == -1) return -1;
			destination[dst] += rc;

			/* advance pointers, here is the reason why the resulting string is shorter */
			dst++; src += 2;
	} 

	/* ensure the string is properly finished with a NULL-character */ 
	destination[dst] = '\0'; 

	return 0; 
}

int _read(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout)
{
	int copied = 0, rc, i;
	fd_set fds;
	struct timeval tv;

	memset(buffer, 0, len);

	while(copied < len) {
		i = MIN(iobuf->level, len - copied);
		memcpy(buffer + copied, iobuf->buffer + IO_BUFFER_SIZE - iobuf->level, i);
		
			//fprintf(stderr, "offset:%d\n", IO_BUFFER_SIZE - iobuf->level);
			//fprintf(stderr, "iobuf:%s\n", iobuf->buffer + IO_BUFFER_SIZE - iobuf->level);
	        
		iobuf->level -= i;
		copied += i;
		if(copied >= len) { 
			/*fprintf(stderr, "%d\n", i);
			fprintf(stderr, "%d\n", iobuf->level);
			fprintf(stderr, "%s\n", iobuf->buffer);
			fprintf(stderr, "%s\n", buffer);
			*/return copied;
		}
	
	/* select will return in case of timeout or new data arrived */
		tv.tv_sec = timeout;
		tv.tv_usec = 0; 
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if((rc = select(fd + 1, &fds, NULL, NULL, &tv)) <= 0) { 
			if(rc < 0) 
				exit(EXIT_FAILURE);
			
			/* this must be a timeout */
			return copied;
		}    
	
		init_iobuffer(iobuf);
	
		/*   
		 ** there should be at least one byte, because select signalled it.
		 ** but: it may happen (very seldomly), that the socket gets closed remotly between
		 ** the select() and the following read. that is the reason for not relying
		 ** on reading at least one byte.
		 **/
		if((iobuf->level = read(fd, &iobuf->buffer, IO_BUFFER_SIZE)) <= 0) { 
				/* an error occured */
				return -1;
		}


		/* align data to the end of the buffer if less than IO_BUFFER_SIZE bytes were read */
		memmove(iobuf->buffer + (IO_BUFFER_SIZE - iobuf->level), iobuf->buffer, iobuf->level);
	}

	return 0;
}

int _readline(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout)
{
	char c = '\0', *out = buffer;
	int i;

	memset(buffer, 0, len);

	for(i = 0; i < len && c != '\n'; i++) {
		if(_read(fd, iobuf, &c, 1, timeout) <= 0) {
				return -1;
		}
		*out++ = c;
	}
	

	return i;
}


void *client_thread(void *arg)
{
//	int sockfd = (int)*arg;
	int sockfd;
	char input_suffixed = 0;
	char buffer[1024] = {0}, *pb = buffer;
	int cnt;

	iobuffer iobuf;
	request req;


	if(arg != NULL) {
			memcpy(&sockfd, arg, sizeof(int));
	}
	else
			return NULL;

	init_iobuffer(&iobuf);
	init_request(&req);

	memset(buffer, 0, sizeof(buffer));

    if((cnt = _readline(sockfd, &iobuf, buffer, sizeof(buffer) - 1, 5)) == -1) {
		close(sockfd);
		return NULL;
	}

	if(strstr(buffer, "get /?action=stream") != NULL) {
		// stream the mjpeg video.
		req.type = A_STREAM;
	}
	else {
	//	send_error(sockfd, 400, "malformed http request");
		send_snapshot(sockfd);
		close(sockfd);
		return NULL;
	}

	/*
	 * parse the rest of the HTTP-requst
	 * the end of the request-header is marked by a single, empty line with "\r\n"
	 */
	do {
		memset(buffer, 0, sizeof(buffer));

		if((cnt = _readline(sockfd, &iobuf, buffer, sizeof(buffer) - 1, 5)) == -1) {
				free_request(&req);
				close(sockfd);
				return NULL;
		}
		
		if(strstr(buffer, "User-Agent: ") != NULL) {
				req.client = strdup(buffer + strlen("User-Agent: "));
		}
		else if(strstr(buffer, "Authorization: Basic ") != NULL) {
				req.credentials = strdup(buffer + strlen("Authorization: Basic "));
				//decodeBase64(req.credentials);
				fprintf(stderr, "user:pass:%s\n", req.credentials);
		}
	} while(cnt > 2 && !(buffer[0] == '\r' && buffer[1] == '\n'));

	// in future add the authorize steps.
	

	// determine what to send.
	switch(req.type) {
	case A_STREAM:
			{
	//			send_stream(sockfd);
			}
		break;
	default:
		fprintf(stderr, "unknown request!\n");
	}
		
	close(sockfd);
	free_request(&req);
	return NULL;
}





int main(int argc, char **argv)
{
	int on; // for port reuse.
	struct addrinfo *aip, *aip2;
	struct addrinfo hints;
	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(struct sockaddr_storage);
	fd_set selectfds;
	int max_fds = 0;
	int err;
	int i;
	char name[NI_MAXHOST];
	int port = 8080;
	
	int serverlen;
	int server[MAX_SD_LEN];


	int clientSock;
	pthread_t client;
	

	struct sockaddr_in *sinp;
	struct sockaddr_in6 *sin6p;
	const char *addr;
	char buf[INET_ADDRSTRLEN];

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(name, sizeof(name), "%d", port);
	if((err = getaddrinfo(NULL, name, &hints, &aip)) != 0) { // name is host secquence
			perror("getaddrinfo:");
			return 0;
	}


	for(i = 0; i < MAX_SD_LEN; i++)
			server[i] = -1;

	i = 0;
	// Create socket.
	for(aip2 = aip; aip2 != NULL; aip2 = aip2->ai_next) {
		

		if(aip2->ai_family == AF_INET) {
			sinp = (struct sockaddr_in *)aip2->ai_addr;
			addr = inet_ntop(AF_INET, &sinp->sin_addr, buf, INET_ADDRSTRLEN);	
			fprintf(stderr, "addr=%s", addr);
			fprintf(stderr, "port=%d\n", ntohs(sinp->sin_port));
		}
		else if(aip2->ai_family == AF_INET6) {
			sin6p = (struct sockaddr_in6 *)aip2->ai_addr;
			addr = inet_ntop(AF_INET6, &sin6p->sin6_addr, buf, INET_ADDRSTRLEN);	
			fprintf(stderr, "addr6=%s", addr);
			fprintf(stderr, "port6=%d\n", ntohs(sin6p->sin6_port));
		}

		if((server[i] = socket(aip2->ai_family, aip2->ai_socktype, 0)) < 0) {
				continue;
		}

		/* ignore "socket already in use" errors.*/
		on = 1;
		if(setsockopt(server[i], SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
				perror("setsockopt(SO_REUSEADDR) failed:");
		}
		
#ifdef IPV6_V6ONLY
		/* ignore "socket already in use" errors.*/
		on = 1;
		if(aip2->ai_family ==AF_INET6 && setsockopt(server[i], IPPROTO_IPV6, IPV6_V6ONLY,
						(const void *)&on, sizeof(on)) < 0) {
				perror("setsockopt(IPV6_V6ONLY) failed");
		}
#endif
	
		// bind.
		if(bind(server[i], aip2->ai_addr, aip2->ai_addrlen) < 0) {
				perror("bind:");
				server[i] = -1;
				continue;
		}

		// listen.
		if(listen(server[i], 10) < 0) {
				perror("listen:");
				server[i] = -1;
		}
		else {
				i++;
				if(i > MAX_SD_LEN) {
					fprintf(stderr, "server socket exceeds!\n");
					i --;
					break;
				}
		}
	}

	serverlen = i;
	if(serverlen < 1) {
			fprintf(stderr, "bind port:%d failed!\n", port);
			return 0;
	}

	while(1) {

			do {
					FD_ZERO(&selectfds);

					for(i = 0; i < MAX_SD_LEN; i++) {
						if(server[i] != -1) {
								FD_SET(server[i], &selectfds);
								if(server[i] > max_fds)
										max_fds = server[i];
						}
					}
					
					err = select(max_fds + 1, &selectfds, NULL, NULL, NULL);

					if(err < 0 && errno != EINTR) {
							perror("select:");
							return 0;
					}
			} while(err <= 0);

			for(i = 0; i < serverlen; i ++) {
					if(server[i] != -1 && FD_ISSET(server[i], &selectfds)) {
							clientSock = accept(server[i], (struct sockaddr *)&client_addr, &addr_len);
							fprintf(stderr, "accept\n");

							if(clientSock == -1) {
									perror("accept:");
									continue;
							}

							if(pthread_create(&client, NULL, &client_thread, &clientSock) != 0) {
									perror("client thread create:");
									close(clientSock);
									continue;
							}
							pthread_detach(client);
					}
			}	
	}





		
}






















