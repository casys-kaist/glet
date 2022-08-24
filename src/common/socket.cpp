#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <tuple>
#include <sstream>
#include <netinet/tcp.h>

#define MAX_BUF_SIZE 65535
#define READ_MAX_BUF_SIZE 512
#define SEND_BUF_SIZE 210000

std::tuple<int, int> socket_connect(int devid, bool iscpu){
	socklen_t to_len, from_len;
	int to_fd, from_fd;
	to_fd=socket(AF_UNIX, SOCK_STREAM, 0);
	from_fd=socket(AF_UNIX, SOCK_STREAM, 0);
	struct sockaddr_un to_addr;
	struct sockaddr_un from_addr;
	bzero(&to_addr, sizeof(to_addr));
	bzero(&from_addr, sizeof(from_addr));
	to_addr.sun_family=AF_UNIX;
	from_addr.sun_family=AF_UNIX;
	std::stringstream to_name;
	std::stringstream from_name;
	if(iscpu){
		to_name<<"/tmp/cpusock_input_"<<devid;
		from_name<<"/tmp/cpusock_output_"<<devid;
	}
	else{
		to_name<<"/tmp/gpusock_input_"<<devid;
		from_name<<"/tmp/gpusock_output_"<<devid;
	}
	strcpy(to_addr.sun_path, to_name.str().c_str());
	strcpy(from_addr.sun_path, from_name.str().c_str());
	to_len=sizeof(to_addr);
	from_len=sizeof(from_addr);
	if (connect(to_fd, (struct sockaddr*)&to_addr, to_len)<0){
		perror("to_fd Error");
		to_fd=-1;
	}
	if (connect(from_fd, (struct sockaddr*)&from_addr, from_len)<0){
		perror("from_fd Error");
		from_fd=-1;
	}
	return std::make_tuple(to_fd, from_fd);
}

int socket_txsize(int socket, int len) {
	int ret = write(socket, (void *)&len, sizeof(int));
	return ret;
}

int socket_send(int socket, char *data, int size, bool debug) {
	int total = 0;
	if (debug) printf("will send total %d bytes over socket %d \n",size,socket);

	while (total < size) {
		int sent = send(socket, data + total, size - total, MSG_NOSIGNAL);
		if (sent <= 0) {
			printf("errno: %s \n",strerror(errno));
			break;
		}
		total += sent;
		if (debug)
			printf("Sent %d bytes of %d total via socket %d\n", total, size, socket);

	}
	return total;
}

// new wrapper for UDP socket
int UDP_socket_send(int socket, char *data, int size, bool debug) {
	int total = 0;
	int sent=0;
	const unsigned int MAX_SIZE = SEND_BUF_SIZE;
	while (total < size) {
		if ( size-total >= MAX_SIZE)
		{
			sent = send(socket, data + total, MAX_SIZE, 0);
		}
		else{
			sent = send(socket, data + total, size-total, 0);
		}
		if (sent <= 0) {
			printf("errno: %s \n",strerror(errno));
			break;
		}
		total += sent;
		if (debug) printf("Sent %d bytes of %d total via socket %d\n", total, size, socket);

	}
	return total;
}

int socket_rxsize(int socket) {
	int size = 0;
	int opt_val=1;
	int stat = read(socket, &size, sizeof(int));
	return (stat < 0) ? -1 : size;
}

int socket_receive(int socket, char *data, int size, bool debug) {
	int rcvd = 0;
	int opt_val=1;
	while (rcvd < size) {
		int got = recv(socket, data + rcvd, size - rcvd, 0);
		if (got == -1){
			printf("socket_recv_error : %s \n",strerror(errno));
			rcvd=-1;
			break;
		}
		if (debug){
			printf("Received %d bytes of %d total via socket %d\n", got, size,socket);
		}
		rcvd += got;
	}
	return rcvd;
}

int client_init(char *hostname, int portno, bool debug) {
	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("ERROR opening socket\n");
		exit(0);
	}
	server = gethostbyname(hostname);
	if (server == NULL) {
		printf("ERROR, no such host\n");
		exit(0);
	}
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
			server->h_length);
	serv_addr.sin_port = htons(portno);
	int on=1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));
	if(sockfd == -1){
		perror("setsockopt");
		exit(1);
	}
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("ERROR connecting\n");
		return -1;
	} else {
		if (debug) printf("Connected to %s:%d\n", hostname, portno);
		return sockfd;
	}
}

int server_init(int portno) {
	int sockfd;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("ERROR opening socket");
		exit(0);
	}
	int on=1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));
	if(sockfd == -1){
		perror("setsockopt");
		exit(1);
	}
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("ERROR on binding\n");
		perror("socket");
		exit(0);
	}
	return sockfd;
}

int socket_close(int socket, bool debug) {
	if (debug) printf("Closing socket %d\n", socket);
	close(socket);
	return 0;
}

