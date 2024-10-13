#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

#define LEN_BUFF		1600
#define LEN_TOPIC		50
#define LEN_CONTENT		1500
#define LEN_ID			10
#define ID_PACKET_LEN	(LEN_ID + 1)

#define INT			0
#define SHORT_REAL	1
#define FLOAT		2
#define STRING		3

#define TCP_MSG_LEN(content_len)	(sizeof(tcp_server_msg) - LEN_CONTENT\
									+ content_len)

#define SUB_MSG_LEN(content_len)	(sizeof(tcp_client_msg) - LEN_TOPIC\
									+ content_len)

int recv_all(int sockfd, void *buffer, size_t len);
int send_all(int sockfd, void *buffer, size_t len);
int get_content_len(char *content);

enum tcp_msg_type {
	SUBSCRIBE,
	UNSUBSCRIBE
};

struct udp_msg {
	char	topic[LEN_TOPIC];
	uint8_t	data_type;
	char	content[LEN_CONTENT];
};

struct tcp_server_msg {
	uint32_t	send_ip;
	uint16_t	send_port;
	char		topic[LEN_TOPIC];
	uint8_t		data_type;
	char 		content[LEN_CONTENT];
};

struct tcp_client_msg {
	tcp_msg_type	type;
	char 			topic[LEN_TOPIC];
};

#endif
