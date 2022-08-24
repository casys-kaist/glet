#ifndef socket_H
#define socket_H

std::tuple<int, int> socket_connect(int devid, bool iscpu);

// returns socket to tx data
int client_init(char* hostname, int portno, bool debug);
// returns socket where to rx data
int server_init(int portno);
// tx len of data
void socket_txsize(int socket, int len);
// receive len of data
int socket_rxsize(int socket);
// send data over socket
int socket_send(int socket, char* data, int size, bool debug);
// receive data over socket
int socket_receive(int socket, char* data, int size, bool debug);
// close the socket
int socket_close(int socket, bool debug);

#endif
