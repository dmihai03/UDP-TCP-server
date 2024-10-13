#include <bits/stdc++.h>

#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

// receive len bytes from sockfd to buffer
int recv_all(int sockfd, void *buffer, size_t len) {
	size_t bytes_received = 0;
	size_t bytes_remaining = len;
	char *buff = (char *)buffer;

	while(bytes_remaining) {
		size_t n_bytes = recv(sockfd, buff + bytes_received,
								bytes_remaining, 0);

		if(n_bytes <= 0)
			return n_bytes;

		bytes_received += n_bytes;
		bytes_remaining -= n_bytes;
	}

	return bytes_received;
}

// send len bytes from buffer to sockfd
int send_all(int sockfd, void *buffer, size_t len) {
	size_t bytes_sent = 0;
	size_t bytes_remaining = len;
	char *buff = (char *)buffer;

	while(bytes_remaining) {
		size_t n_bytes = send(sockfd, buff + bytes_sent,
								bytes_remaining, 0);

		if(n_bytes <= 0)
			return n_bytes;

		bytes_sent += n_bytes;
		bytes_remaining -= n_bytes;
	}

	return bytes_sent;
}

int get_content_len(char *content) {
	for (int i = 0; i < LEN_CONTENT; i++) {
		if (content[i] == '\0') {
			return i + 1;
		}
	}

	return LEN_CONTENT;
}
