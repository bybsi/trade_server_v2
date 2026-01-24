#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stddef.h>
#include <pthread.h>

#include "database.h"
#include "logger.h"
#include "sse_server.h"
#include "sse_client_writer.h"

#define MAX_EVENTS 100


static int verbose = 0;

volatile sig_atomic_t stop_signal_received = 0;
void stop_server_handler(int signal) {
	stop_signal_received = 1;
}

static int make_socket_non_blocking(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		fprintf(stderr, "fcntl F_GETFL failed\n");
		return -1;
	}
	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		fprintf(stderr, "fcntl F_SETFL failed\n");
		return -1;
	}
	return 0;
}

static int send_sse_headers(int client_fd) {
	const char *headers = 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/event-stream\r\n"
		"Cache-Control: no-cache\r\n"
		"Connection: keep-alive\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"\r\n";
	ssize_t bytes = write(client_fd, headers, strlen(headers));
	return (bytes == (ssize_t)strlen(headers)) ? 0 : -1;
}

static void sse_server_destroy(st_sse_server_t *server) {
	unsigned short i;
	logger_write(server->logger, "sse_server_destroy called");
	
	client_writer_stop(server->client_writer);
	client_writer_destroy(server->client_writer);
	free(server->client_writer);
	free(server);
	
	logger_close(server->logger);

}

st_sse_server_t * sse_server_init(
	unsigned short port, 
	unsigned short data_queue_size
){
	unsigned short i;
	st_sse_server_t * server = malloc(sizeof(st_sse_server_t));
	server->client_writer = client_writer_init(data_queue_size);
	if (!server->client_writer) {
		printf("Could not initialize client_writer\n");
		return NULL;
	}
	server->port = port;
	server->logger = logger_init("sse_server.log");
	return server;
}

void *server_thread (void *server_vp) {
	int server_fd, epoll_fd, signal_fd, epoll_result;
	sigset_t block_mask, old_mask;
	struct sigaction sa;
	struct sockaddr_in address;
	struct epoll_event event, stop_event, events[MAX_EVENTS];
	st_sse_server_t *server = (st_sse_server_t *) server_vp;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		fprintf(stderr, "Could not create socket\n");
		exit(255);
	}

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		fprintf(stderr, "setsockopt failed\n");
		exit(255);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(server->port);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		fprintf(stderr, "bind failed\n");
		exit(255);
	}

	if (listen(server_fd, SOMAXCONN) < 0) {
		fprintf(stderr, "listen failed\n");
		exit(255);
	}

	if (make_socket_non_blocking(server_fd) < 0)
		exit(255);

	epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		fprintf(stderr, "epoll_create1 failed\n");
		exit(255);
	}

	event.events = EPOLLIN;
	event.data.fd = server_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0) {
		fprintf(stderr, "epoll_ctl add server_fd failed\n");
		exit(255);
	}

	signal(SIGINT, stop_server_handler);
	signal(SIGTERM, stop_server_handler);
	sa.sa_handler = stop_server_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT);
	sigaddset(&block_mask, SIGTERM);

	if (sigprocmask(SIG_BLOCK, &block_mask, &old_mask) == -1) {
		fprintf(stderr, "Could not setup epoll exit signaling\n");
		exit(255);
	}
	/*
	signal_fd = signalfd(-1, &mask, 0);
	if (signal_fd == -1) {
		fprintf(stderr, "Could not setup signal_fd\n");
		exit(255);
	}
	
	stop_event.data.fd = signal_fd;
	stop_event.events = EPOLLIN;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_fd, &stop_event) == -1) {
		fprintf(stderr, "epoll_ctl add signal_fd failed\n");
		exit(255);
	}
	*/

	printf("SSE Server started on port %d\n", server->port);

	while (!stop_signal_received) {
		
		//epoll_result = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		epoll_result = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &old_mask);
		if (epoll_result == -1) {
			if (errno == EINTR) {
				// stop_signal_received should now be 1
				continue;
			} else {
				logger_write(server->logger, "epoll_pwait error");
				break;
			}
		}

		for (int i = 0; i < epoll_result; i++) {
			if (events[i].data.fd == server_fd) {
				// Handle new connection
				struct sockaddr_in client_addr;
				socklen_t client_len = sizeof(client_addr);
				int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
				
				if (client_fd < 0) {
					if (errno != EAGAIN && errno != EWOULDBLOCK) {
						logger_write(server->logger, 
							"Error accepting connection errno: %d",
							errno);
					}
					continue;
				}

				if (make_socket_non_blocking(client_fd) < 0) {
					close(client_fd);
					continue;
				}

				event.events = EPOLLIN | EPOLLET;
				event.data.fd = client_fd;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0) {
					logger_write(server->logger, "epoll ctl add error");
					close(client_fd);
					continue;
				}

				if (send_sse_headers(client_fd) < 0) {
					logger_write(server->logger, "Failed to send SSE headers");
					close(client_fd);
					continue;
				}
				
				logger_write(server->logger, "Client connected");
				client_writer_add_client(server->client_writer, client_fd);
			} else {
				// Handle client data
				// Should never get here after headers are received since
				// this is a write only server (SSE).
				char buffer[1024];
				ssize_t count = read(events[i].data.fd, buffer, sizeof(buffer));
				if (count <= 0) {
					if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
						logger_write(server->logger,
							"Error reading client data: %d",
							errno);
					}
					logger_write(server->logger, "Client disconnected");
					close(events[i].data.fd);
					continue;
				}
				if (verbose)
					printf("Headers: %s\n", buffer);
			}
		}
	}
	
	sse_server_destroy(server);
	close(epoll_fd);
	close(server_fd);
	return 0;
} 

pthread_t sse_server_start(st_sse_server_t *server) {
	pthread_t server_tid;
	client_writer_start(server->client_writer);
	pthread_create(&server_tid, NULL, server_thread, server);
	return server_tid;
}

void sse_server_queue_data(st_sse_server_t * server, char *data) {
	client_writer_queue_data(server->client_writer, data);
}

void sse_server_stop(st_sse_server_t *server) {
	logger_write(server->logger, "sse_server_stop called");
	raise(SIGTERM);
}

