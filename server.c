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
#include "server.h"


void init_request(request *req)
{
	req->type        = A_UNKNOWN;
	req->type        = A_UNKNOWN;
	req->parameter   = NULL;
	req->client      = NULL;
	req->credentials = NULL;
}

void init_iobuffer(iobuffer *iobuf)
{
	memset(iobuf->buffer, 0, sizeof(iobuf->buffer));
	iobuf->level = 0;
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
			
			sprintf(buffer, "Http/1.0 400 Bad Request\r\n" \
					"Content-type: text/plain\r\n" \
					"Contentp-Length:"
					STD_HEADER \
					"\r\n" \
					"400: Not Found!\r\n" \
					"%s\r\n", message);
			break;
	default:
			break;
	}

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
		// first send error.
		memset(buffer, 0, sizeof(buffer));
		sprintf(buffer,"HTTP/1.0 404 NOT FOUND\r\n"\
				"Content-Type: text/html\r\n"\
				"Content-Length: 328\r\n"\
				"Date: Tue, 26 Feb 2013 07:00:00 CMT\r\n"\
				"Server: lighthttpd/1.4.28\r\n"\
				"X-Cache: MISS from wiki.bdwm.net\r\n"\
				"via: 1.0 wiki.bdwm.net (squid/3.1.12)\r\n"\
				"Connection: keep-alive\r\n"\
				"\r\n"\
				"<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"\
				"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n"\
         			"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"\
				"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"\
				"<head>\n"\
   				"<title>404 - Not Found</title>\n"\
    			"</head>\n"\
 				"<body>\n"\
  				"<h1>404 - Not Found</h1>\n"\
   				"</body>\n"\
				"</html>\n");
		char test[] ="<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"\
				"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n"\
         			"\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"\
				"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"\
				"<head>\n"\
   				"<title>404 - Not Found</title>\n"\
    			"</head>\n"\
 				"<body>\n"\
  				"<h1>404 - Not Found</h1>\n"\
   				"</body>\n"\
				"</html>\n";
		
		fprintf(stderr, "%s\n", buffer);
		fprintf(stderr, "%d\n", strlen(test));
		fprintf(stderr, "%d\n", sizeof(buffer));

		if(write(sockfd, buffer, strlen(buffer)) == -1) {
			perror("send:");
			close(sockfd);
			return NULL;
		}
	//	send_error(sockfd, 400, "malformed http request");
	}

	/*
	 * parse the rest of the HTTP-requst
	 * the end of the request-header is marked by a single, empty line with "\r\n"
	 */
	//do {
	//	memset(buffer, 0, sizeof(buffer));

	//	if((cnt = _readline(sockfd, &iobuf, buffer



	/* determine what to deliver */
/*	if(strstr(buffer, "get /?action=snapshot") != NULL) {
			req.type = a_snapshot;
	} else if((strstr(buffer, "get /cam") != NULL) && (strstr(buffer, ".jpg") != NULL)) {
			req.type = a_snapshot;
			input_suffixed = 255;
	} else if(strstr(buffer, "get /?action=stream") != NULL) {
			input_suffixed = 255;
			req.type = a_stream;
	} else if((strstr(buffer, "get /cam") != NULL) && (strstr(buffer, ".mjpg") != NULL)) {
			req.type = a_stream;
			input_suffixed = 255;
	} else if((strstr(buffer, "get /input") != NULL) && (strstr(buffer, ".json") != NULL)) {
			req.type = a_input_json;
			input_suffixed = 255;
	} else if((strstr(buffer, "get /output") != NULL) && (strstr(buffer, ".json") != NULL)) {
			req.type = a_output_json;
			input_suffixed = 255;
	} else if(strstr(buffer, "get /program.json") != NULL) {
			req.type = a_program_json;
			input_suffixed = 255;
	} else if(strstr(buffer, "get /?action=command") != NULL) {
			int len;
			req.type = a_command;

			// advance by the length of known string 
			if((pb = strstr(buffer, "get /?action=command")) == NULL) {
				//	send_error(lcfd.fd, 400, "malformed http request");
				//	close(lcfd.fd);
					return NULL;
			}
			pb += strlen("get /?action=command"); // a pb points to thestring after the first & after command

			// only accept certain characters 
			len = min(max(strspn(pb, "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz_-=&1234567890%./"), 0), 100);

			req.parameter = malloc(len + 1);
			if(req.parameter == NULL) {
					exit(exit_failure);
			}
			memset(req.parameter, 0, len + 1);
			strncpy(req.parameter, pb, len);

			if(unescape(req.parameter) == -1) {
					free(req.parameter);
				//	send_error(lcfd.fd, 500, "could not properly unescape command parameter string");
				//	LOG("could not properly unescape command parameter string\n");
				//	close(lcfd.fd);
					return NULL;
			}

	} else {
			int len;

			req.type = A_FILE;

			if((pb = strstr(buffer, "GET /")) == NULL) {
				//	send_error(lcfd.fd, 400, "Malformed HTTP request");
				//	close(lcfd.fd);
					return NULL;
			}

			pb += strlen("GET /");
			len = MIN(MAX(strspn(pb, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-1234567890"), 0), 100);
			req.parameter = malloc(len + 1);
			if(req.parameter == NULL) {
					exit(EXIT_FAILURE);
			}
			memset(req.parameter, 0, len + 1);
			strncpy(req.parameter, pb, len);

	}*/
/*	numbytes = _read(sockfd, &iobuf, buf, 100, 5);
	fprintf(stderr, "%s", buf);
		

	if((numbytes = recv(sockfd, buf, 1024, 0)) == -1) {
		perror("recv");
	}
	else {
		buf[numbytes] = '\0';
		fprintf(stderr, "%s", buf);
	}	


	if(send(sockfd, "Hello world!\n", 14, 0) == -1) {
			perror("send:");
			close(sockfd);
			return NULL;
	}*/
	close(sockfd);
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






















