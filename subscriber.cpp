#include <bits/stdc++.h>

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

#define ID			argv[1]
#define IP_SERVER	argv[2]
#define PORT_SERVER	argv[3]

using namespace std;

vector<pollfd> fds;

static void sanity_checks(int argc, char **argv) {
	sockaddr_in tmp;

	DIE(argc != 4, "Usage: ./subscriber <ID> <IP_SERVER> <PORT_SERVER>");

	DIE(strlen(ID) > LEN_ID, "ID too long");

	DIE(inet_aton(IP_SERVER, &tmp.sin_addr) == 0, "Given IP is invalid");

	DIE(atoi(PORT_SERVER) < 0 || atoi(PORT_SERVER) > 65535, "Given PORT is invalid");
}

static void init_socket(int &sockfd, sockaddr_in &server_addr, char** argv) {
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	DIE(sockfd < 0, "socket");

	// make reusable address
	int tmp = 1;
	DIE(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int)) < 0,
		"setsockopt reusable");

	//disable Nagle algorithm
	DIE(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &tmp,
		sizeof(int)) < 0, "setsockopt tcp_nodelay");

	memset(&server_addr, 0, sizeof(sockaddr_in));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(PORT_SERVER));

	DIE(inet_aton(IP_SERVER, &server_addr.sin_addr) == 0, "inet_aton");
}

static void pretty_DIE(int sockfd, const char *msg) {
	close(sockfd);
	DIE(true, msg);
}

static void send_ID(int sockfd, char **argv) {
	char buff[ID_PACKET_LEN];

	memset(buff, 0, ID_PACKET_LEN);
	strcpy(buff, ID);

	// send the length of the ID
	size_t len = strlen(buff);
	int res = send_all(sockfd, &len, sizeof(size_t));
	if (res < 0) {
		pretty_DIE(sockfd, "send_all");
	}

	// send the ID
	res = send_all(sockfd, buff, len);
	if (res < 0) {
		pretty_DIE(sockfd, "send_all");
	}
}

static void add_fd(int sockfd) {
	pollfd tmp;
	tmp.fd = sockfd;
	tmp.events = POLLIN;

	fds.push_back(tmp);
}

static void init_pollfd(int sockfd) {
	add_fd(sockfd);
	add_fd(STDIN_FILENO);
}

static bool valid_command(const char *command) {
	if (strncmp(command, "exit", sizeof("exit") - 1) == 0) {
		return true;
	}

	if (strncmp(command, "subscribe", sizeof("subscribe") - 1) == 0) {
		return true;
	}

	if (strncmp(command, "unsubscribe", sizeof("unsubscribe") - 1) == 0) {
		return true;
	}

	return false;
}

static void parse_command(char *command, int sockfd) {
	int res;

	tcp_client_msg msg;
	memset(&msg, 0, sizeof(tcp_client_msg));

	uint8_t request_recieved;

	if (strncmp(command, "exit", sizeof("exit") - 1) == 0) {
		close(sockfd);
		exit(0);
	}

	else if (strncmp(command, "subscribe", sizeof("subscribe") - 1) == 0) {
		// first token -> "subscribe"
		char *tmp = strtok(command, " ");

		// second token -> topic
		tmp = strtok(NULL, " ");

		strcpy(msg.topic, tmp);
		msg.type = SUBSCRIBE;

		size_t len = SUB_MSG_LEN(strlen(msg.topic));

		// send to server the message length
		res = send_all(sockfd, &len, sizeof(size_t));
		if (res < 0) {
			pretty_DIE(sockfd, "send_all");
		}

		// send to server the subscribe message
		res = send_all(sockfd, &msg, len);
		if (res < 0) {
			pretty_DIE(sockfd, "send_all");
		}

		// receive the server's response
		res = recv_all(sockfd, &request_recieved, sizeof(uint8_t));
		if (res < 0) {
			pretty_DIE(sockfd, "recv_all");
		}

		if (request_recieved) {
			fprintf(stdout, "Subscribed to topic %s\n", msg.topic);
		}
	}

	else if (strncmp(command, "unsubscribe", sizeof("unsubscribe") - 1) == 0) {
		// first token -> "unsubscribe"
		char *tmp = strtok(command, " ");

		// second token -> topic
		tmp = strtok(NULL, " ");

		strcpy(msg.topic, tmp);
		msg.type = UNSUBSCRIBE;

		size_t len = SUB_MSG_LEN(strlen(msg.topic));

		// send to server the message length
		res = send_all(sockfd, &len, sizeof(size_t));
		if (res < 0) {
			pretty_DIE(sockfd, "send_all");
		}

		// send to server the unsubscribe message
		res = send_all(sockfd, &msg, len);
		if (res < 0) {
			pretty_DIE(sockfd, "send_all");
		}

		// receive the server's response
		res = recv_all(sockfd, &request_recieved, sizeof(uint8_t));
		if (res < 0) {
			pretty_DIE(sockfd, "recv_all");
		}

		if (request_recieved) {
			fprintf(stdout, "Unsubscribed from topic %s\n", msg.topic);
		}
	}
}

static void interpret_message(tcp_server_msg *msg) {
	if (msg->data_type == INT) {
		uint32_t value = *(uint32_t *)((uint8_t *)((uint8_t *)msg->content +
							sizeof(uint8_t)));

		uint8_t sign = *(uint8_t *)msg->content;

		// convert to host order
		value = ntohl(value);

		char *ip = inet_ntoa(*(in_addr *)&msg->send_ip);

		if (sign == 1) {
			value = -value;
		}

		fprintf(stdout, "%s:%d - %s - INT - %d\n", ip, msg->send_port,
				msg->topic, value);
	}

	else if (msg->data_type == SHORT_REAL) {
		float value = *(uint16_t *)msg->content;

		// convert to host order
		value = ntohs(value);

		// get the real value
		value /= 100.0F;

		char *ip = inet_ntoa(*(in_addr *)&msg->send_ip);

		fprintf(stdout, "%s:%d - %s - SHORT_REAL - %0.2f\n", ip, msg->send_port,
			msg->topic, (float)value);
	}

	else if (msg->data_type == FLOAT) {
		uint8_t sign = *(uint8_t *)msg->content;

		uint32_t value = *(uint32_t *)((uint8_t *)((uint8_t *)msg->content +
							sizeof(uint8_t)));

		uint8_t power = *(uint8_t *)((uint8_t *)msg->content + sizeof(uint8_t)
						+ sizeof(uint32_t));

		// convert to host order
		value = ntohl(value);

		// get the real value
		double res = value / pow(10, power);

		char *ip = inet_ntoa(*(in_addr *)&msg->send_ip);

		if (sign == 1) {
			fprintf(stdout, "%s:%d - %s - FLOAT - -%.*f\n", ip, msg->send_port,
				msg->topic, power, res);
		} else {
			fprintf(stdout, "%s:%d - %s - FLOAT - %.*f\n", ip, msg->send_port,
				msg->topic, power, res);
		}
	}

	else if (msg->data_type == STRING) {
		int len = get_content_len((char *)msg->content);
		char buff[len + 1];
		memset(buff, 0, len + 1);

		memcpy(buff, msg->content, len);

		char *ip = inet_ntoa(*(in_addr *)&msg->send_ip);

		fprintf(stdout, "%s:%d - %s - STRING - %s\n", ip, msg->send_port,
			msg->topic, buff);
	}
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	sanity_checks(argc, argv);

	int sockfd;
	sockaddr_in server_addr;

	init_socket(sockfd, server_addr, argv);

	// connect to server
	DIE(connect(sockfd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0,
		"connect");

	// send to server the client ID
	send_ID(sockfd, argv);

	/* 
		receive the server's response (if ID is used or not)
		the response is a "boolean" value (1 if the ID is used,
		0 otherwise)
	*/
	uint8_t is_ID_used;

	int res = recv_all(sockfd, &is_ID_used, sizeof(uint8_t));
	if (res < 0) {
		pretty_DIE(sockfd, "recv_all");
	}

	// if ID is used shut down the clitent
	if (is_ID_used) {
		close(sockfd);

		return 0;
	}

	init_pollfd(sockfd);

	while (true) {
		int res;
		char buff[LEN_BUFF];

		memset(buff, 0, LEN_BUFF);

		res = poll(fds.data(), fds.size(), -1);

		if (res < 0) {
			pretty_DIE(sockfd, "poll");
		}

		for (const auto it : fds) {
			if (it.revents & POLLIN) {
				if (it.fd == STDIN_FILENO) {
					fgets(buff, LEN_BUFF, stdin);

					// remove newline
					buff[strlen(buff) - 1] = '\0';

					if (!valid_command(buff)) {
						fprintf(stderr, "Invalid command.\n");
						continue;
					}

					parse_command(buff, sockfd);
				}

				else if (it.fd == sockfd) {
					int len;

					// receive the length of the message
					int res = recv_all(sockfd, &len, sizeof(int));
					if (res < 0) {
						pretty_DIE(sockfd, "recv_all");
					}

					if (res == 0) {
						close(sockfd);
						exit(0);
					}

					// receive the message
					res = recv_all(sockfd, buff, len);
					if (res < 0) {
						pretty_DIE(sockfd, "recv_all");
					}

					if (res == 0) {
						close(sockfd);
						exit(0);
					}

					tcp_server_msg *msg = (tcp_server_msg *)buff;

					interpret_message(msg);
				}
			}
		}
	}

	return 0;
}
