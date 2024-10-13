#include <bits/stdc++.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

#define PORT			argv[1]
#define MAX_CLIENTS		50
#define EXIT_MSG_LEN	sizeof("exit")

using namespace std;

struct subscriber {
	int				sockfd;
	string			id;
	bool			is_connected;
	vector<string>	topics;
};

// operator used for removing a pollfd from vector
bool operator==(const pollfd& fd1, const pollfd& fd2) {
	return fd1.fd == fd2.fd;
}

vector<subscriber> subscribers;
vector<pollfd> fds;

static void sanity_checks(int argc, char **argv) {
	DIE(argc != 2, "Usage: ./server <PORT>");

	DIE(atoi(PORT) < 0 || atoi(PORT) > 65535, "Given PORT is invalid");
}

static void close_clients() {
	for (const auto it : subscribers) {
		close(it.sockfd);
	}
}

static void pretty_DIE(int tcp_fd, int udp_fd, const char *msg) {
	close(tcp_fd);
	close(udp_fd);
	close_clients();
	DIE(true, msg);
}

static void init_tcp_sock(int &sockfd, sockaddr_in &addr, char **argv) {
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	DIE(sockfd < 0, "socket");

	// make reusable address
	int tmp = 1;
	DIE(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int)) < 0,
		"setsockopt reusable");

	//disable Nagle algorithm
	DIE(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &tmp,
		sizeof(int)) < 0, "setsockopt tcp_nodelay");

	memset(&addr, 0, sizeof(sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(PORT));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind the socket to the given port
	DIE(bind(sockfd, (sockaddr*)&addr, sizeof(sockaddr_in)) < 0, "bind");

	DIE(listen(sockfd, MAX_CLIENTS) < 0, "listen");
}

static void init_udp_sock(int &sockfd, sockaddr_in &addr, char **argv) {
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	DIE(sockfd < 0, "socket");

	// make reusable address
	int tmp = 1;
	DIE(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int)) < 0,
		"setsockopt");

	memset(&addr, 0, sizeof(sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(PORT));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind the socket to the given port
	DIE(bind(sockfd, (sockaddr*)&addr, sizeof(sockaddr_in)) < 0, "bind");
}

static void add_fd(int sockfd) {
	pollfd tmp;
	tmp.fd = sockfd;
	tmp.events = POLLIN;

	fds.push_back(tmp);
}

static void init_pollfd(int tcp_sockfd, int udp_sockfd) {
	add_fd(tcp_sockfd);
	add_fd(udp_sockfd);
	add_fd(STDIN_FILENO);
}

static void add_subscriber(int sockfd, string id) {
	subscriber tmp;
	tmp.sockfd = sockfd;
	tmp.id = id;
	tmp.is_connected = true;

	subscribers.push_back(tmp);
}

static void connect_subscriber(int sockfd, string id) {
	for (auto &s : subscribers) {
		if (s.id == id) {
			s.sockfd = sockfd;
			s.is_connected = true;
			return;
		}
	}

	add_subscriber(sockfd, id);
}

static bool is_id_used(string id) {
	for (const auto it : subscribers) {
		if (it.id == id && it.is_connected) {
			return true;
		}
	}

	return false;
}

static void my_handler(sig_atomic_t s) {
	close_clients();

	exit(0);
}

static string get_ID(int sockfd) {
	for (auto it : subscribers) {
		if (it.sockfd == sockfd) {
			return it.id;
		}
	}

	return NULL;
}

static bool is_subbed(string topic, vector<string> topics) {
	for (auto it : topics) {
		if (it == topic) {
			return true;
		}

		// verify wildcard
		if (it.find('*') != string::npos || it.find('+') != string::npos) {
			string pattern = regex_replace(it, regex("\\*"), ".*");
			pattern = regex_replace(pattern, regex("\\+"), "[^/]*");

			regex rx(pattern);

			if (regex_match(topic, rx)) {
				return true;
			}
		}
	}

	return false;
}

static int get_tcp_content_len(tcp_server_msg *msg) {
	int res;

	switch(msg->data_type) {
	case INT:
		res = sizeof(uint8_t) + sizeof(uint32_t);
		break;

	case SHORT_REAL:
		res = sizeof(uint16_t);
		break;

	case FLOAT:
		res = sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t);
		break;

	case STRING:
		res = get_content_len(msg->content);
		break;
	}

	return res;
}

static void interpret_message(tcp_client_msg *msg, string id) {
	if (msg->type == SUBSCRIBE) {
		for (auto &s : subscribers) {
			if (s.id == id && s.is_connected) {
				s.topics.push_back(msg->topic);

				// send to client confirmation message
				uint8_t ok = 1;
				
				int res = send_all(s.sockfd, &ok, sizeof(uint8_t));
				DIE(res < 0, "send_all");

				return;
			}
		}
	} else if (msg->type == UNSUBSCRIBE) {
		for (auto &s : subscribers) {
			if (s.id == id && s.is_connected) {
				s.topics.erase(remove(s.topics.begin(), s.topics.end(),
								msg->topic));

				// send to client confirmation message
				uint8_t ok = 1;

				int res = send_all(s.sockfd, &ok, sizeof(uint8_t));
				DIE(res < 0, "send_all");

				return;
			}
		}
	}

	// send to client error message
	bool ok = false;

	for (auto &s : subscribers) {
		if (s.id == id && s.is_connected) {
			int res = send_all(s.sockfd, &ok, sizeof(bool));
			DIE(res < 0, "send_all");

			break;
		}
	}
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	sanity_checks(argc, argv);

	int tcp_sockfd, udp_sockfd;
	sockaddr_in tcp_addr, udp_addr;

	init_tcp_sock(tcp_sockfd, tcp_addr, argv);
	init_udp_sock(udp_sockfd, udp_addr, argv);

	init_pollfd(tcp_sockfd, udp_sockfd);

	while (true) {
		int res = poll(fds.data(), fds.size(), -1);

		if (res < 0) {
			pretty_DIE(tcp_sockfd, udp_sockfd, "poll");
		}

		// if the server is shut down, close all clients and exit
		signal(SIGINT, my_handler);

		for (const auto it : fds) {
			if (it.revents & POLLIN) {
				if (it.fd == STDIN_FILENO) {
					char buff[EXIT_MSG_LEN];
					memset(buff, 0, EXIT_MSG_LEN);

					fgets(buff, EXIT_MSG_LEN, stdin);

					if (strncmp(buff, "exit", 4) == 0) {
						close(tcp_sockfd);
						close(udp_sockfd);
						close_clients();
						exit(0);
					} else {
						fprintf(stderr, "Invalid command.\n");
						continue;
					}
				}

				else if (it.fd == tcp_sockfd) {
					sockaddr_in client_addr;
					socklen_t client_len = sizeof(sockaddr_in);

					int client_fd = accept(tcp_sockfd, (sockaddr*)&client_addr,
											&client_len);

					if (client_fd < 0) {
						pretty_DIE(tcp_sockfd, udp_sockfd, "accept");
					}

					//disable Nagle algorithm
					int tmp = 1;
					res = setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &tmp,
										sizeof(int));
					if (res < 0) {
						pretty_DIE(tcp_sockfd, udp_sockfd, "setsockopt");
					}

					char buff[ID_PACKET_LEN];

					// receive the ID length
					size_t len;
					res = recv_all(client_fd, &len, sizeof(size_t));
					if (res < 0) {
						pretty_DIE(tcp_sockfd, udp_sockfd, "recv_all");
					}

					// receive client ID
					memset(buff, 0, ID_PACKET_LEN);
					res = recv_all(client_fd, buff, len);

					if (res < 0) {
						pretty_DIE(tcp_sockfd, udp_sockfd, "recv_all");
					}

					string id(buff);

					// check if ID is used and send the response to the client
					uint8_t is_ID_used = 1;
					if (is_id_used(id)) {
						fprintf(stdout, "Client %s already connected.\n",
							id.c_str());

						res = send_all(client_fd, &is_ID_used, sizeof(uint8_t));
						if (res < 0) {
							pretty_DIE(tcp_sockfd, udp_sockfd, "send_all");
						}

						close(client_fd);
						continue;
					}
				
					is_ID_used = 0;
					res = send_all(client_fd, &is_ID_used, sizeof(uint8_t));
					if (res < 0) {
						pretty_DIE(tcp_sockfd, udp_sockfd, "send_all");
					}

					add_fd(client_fd);
					connect_subscriber(client_fd, id);

					fprintf(stdout, "New client %s connected from %s:%d.\n",
						id.c_str(), inet_ntoa(client_addr.sin_addr),
						ntohs(client_addr.sin_port));
				}

				else if (it.fd == udp_sockfd) {
					sockaddr_in client_addr;
					socklen_t client_len = sizeof(sockaddr_in);

					char buff[LEN_BUFF];
					memset(buff, 0, LEN_BUFF);

					// receive the udp message from the client
					res = recvfrom(udp_sockfd, buff, LEN_BUFF, 0,
									(sockaddr*)&client_addr, &client_len);

					if (res < 0) {
						pretty_DIE(tcp_sockfd, udp_sockfd, "recvfrom");
					}

					udp_msg *msg = (udp_msg*)buff;

					tcp_server_msg tcp_msg;
					memset(&tcp_msg, 0, sizeof(tcp_server_msg));

					tcp_msg.send_ip = client_addr.sin_addr.s_addr;
					tcp_msg.send_port = client_addr.sin_port;
					memcpy(tcp_msg.topic, msg->topic, LEN_TOPIC);
					tcp_msg.data_type = msg->data_type;

					memcpy(tcp_msg.content, msg->content, LEN_CONTENT);

					string topic(tcp_msg.topic);

					int len = TCP_MSG_LEN(get_tcp_content_len(&tcp_msg));

					for (auto s : subscribers) {
						if (is_subbed(topic, s.topics) && s.is_connected) {
							// first, send the message length
							res= send_all(s.sockfd, &len, sizeof(int));

							if (res < 0) {
								pretty_DIE(tcp_sockfd, udp_sockfd, "send_all");
							}

							// send the message
							res = send_all(s.sockfd, &tcp_msg, len);

							if (res < 0) {
								pretty_DIE(tcp_sockfd, udp_sockfd, "send_all");
							}
						}
					}
				}

				else {
					char buff[LEN_BUFF];
					memset(buff, 0, LEN_BUFF);

					string id = get_ID(it.fd);

					size_t len;

					// receive the lenght of tcp message
					res = recv_all(it.fd, &len, sizeof(size_t));
					if (res < 0) {
						pretty_DIE(tcp_sockfd, udp_sockfd, "recv_all");
					}

					// receive the tcp message from the client
					res = recv_all(it.fd, buff,len);
					if (res < 0) {
						pretty_DIE(tcp_sockfd, udp_sockfd, "recv_all");
					}

					// the client disconnected
					if (res == 0) {
						for (auto &s : subscribers) {
							if (s.id == id) {
								s.is_connected = false;
								s.sockfd = -1;
								break;
							}
						}

						close(it.fd);

						fprintf(stdout, "Client %s disconnected.\n",
								id.c_str());

						fds.erase(remove(fds.begin(), fds.end(), it), fds.end());
						continue;
					}

					tcp_client_msg *msg = (tcp_client_msg*)buff;

					interpret_message(msg, id);
				}
			}
		}
	}

	return 0;
}
