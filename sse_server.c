#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <database.h>

#define MAX_EVENTS 10
#define PORT 6262
#define BUFFER_SIZE 1024

static int make_socket_non_blocking(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		perror("fcntl F_GETFL");
		return -1;
	}
	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		perror("fcntl F_SETFL");
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

static int send_sse_event(int client_fd, const char *data) {
	char buffer[BUFFER_SIZE];
	snprintf(buffer, sizeof(buffer), "data: %s\n\n", data);
	ssize_t bytes = write(client_fd, buffer, strlen(buffer));
	return (bytes == (ssize_t)strlen(buffer)) ? 0 : -1;
}

static void log_event(const char *event_type, const char *event_data) {
}

int main() {
	int server_fd, epoll_fd;
	struct sockaddr_in address;
	struct epoll_event event, events[MAX_EVENTS];

	if (db_init() < 0) {
		fprintf(stderr, "Failed to initialize MySQL\n");
		exit(255);
	}

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		perror("Could not create socket.");
		exit(255);
	}

	int opt = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("setsockopt failed.");
		exit(255);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed.");
		exit(255);
	}

	if (listen(server_fd, SOMAXCONN) < 0) {
		perror("listen failed.");
		exit(255);
	}

	if (make_socket_non_blocking(server_fd) < 0)
		exit(255);

	epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		perror("epoll_create1 failed.");
		exit(255);
	}

	event.events = EPOLLIN;
	event.data.fd = server_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0) {
		perror("epoll_ctl");
		exit(255);
	}

	printf("SSE Server started on port %d\n", PORT);

	while (1) {
		// Need to add break case and SIGINT SIGTERM SIGKILL handling

		int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		for (int i = 0; i < n; i++) {
			if (events[i].data.fd == server_fd) {
				// Handle new connection
				struct sockaddr_in client_addr;
				socklen_t client_len = sizeof(client_addr);
				int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
				
				if (client_fd < 0) {
					if (errno != EAGAIN && errno != EWOULDBLOCK) {
						perror("accept");
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
					perror("epoll_ctl");
					close(client_fd);
					continue;
				}

				if (send_sse_headers(client_fd) < 0) {
					perror("Failed to send SSE headers");
					close(client_fd);
					continue;
				}
				printf("New client connected\n");
				
				log_event("connection", "New client connected");

				// Send latest data.
				/*
				if (latest_data) {
					if (send_sse_event(client_fd, latest_data) < 0) {
						perror("Failed to send latest data");
						free(latest_data);
						close(client_fd);
						continue;
					}
					free(latest_data);
				}
				*/

			} else {
				// Handle client data
				char buffer[BUFFER_SIZE];
				ssize_t count = read(events[i].data.fd, buffer, sizeof(buffer));
				
				if (count <= 0) {
					if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
						perror("read");
					}
					printf("Client disconnected\n");
					log_event("disconnection", "Client disconnected");
					close(events[i].data.fd);
					continue;
				}

				// send latest data
				//time_t now = time(NULL);
				//char timestamp[64];
				//strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
				
				//if (send_sse_event(events[i].data.fd, timestamp) < 0) {
				//	perror("Failed to send timestamp");
				//	close(events[i].data.fd);
				//	continue;
				//}
			}
		}
	}

	db_close();
	close(server_fd);
	close(epoll_fd);
	return 0;
} 
