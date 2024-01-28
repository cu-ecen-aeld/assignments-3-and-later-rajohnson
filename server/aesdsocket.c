#include <syslog.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <iso646.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>


// get sockaddr, IPv4 or IPv6 -- from Beej's guide
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char **argv) {
	openlog("aesdsocket", 0, LOG_USER);

	// check that the arguments exist
	if(argc > 2) {
		printf("todo - remove. too many args\n");
		syslog(LOG_ERR, "too many arguments - usage: %s [-d]", argv[0]);
		return 1;
	}

	if(argc == 2) {
		if(strncmp(argv[1], "-d", 3) == 0) {
			// todo run in daemon mode.
			printf("todo - daemon mode.\n");
		} else {
			printf("todo - remove - bad args\n");
			syslog(LOG_ERR, "invalid argument - usage: %s [-d]", argv[0]);
			return 1;
		}
	}

	// todo  Opens a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.
	struct addrinfo hints;
	struct addrinfo* servinfo;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if(getaddrinfo(NULL, "9000", &hints, &servinfo) != 0) {
		syslog(LOG_ERR, "getaddrinfo failed");
		return -1;
	}
	// todo need to call freeaddrinfo(servinfo); before exiting after this
	int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if(sockfd == -1) {
		syslog(LOG_ERR, "error opening socket");
        return -1;
	}
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    	syslog(LOG_ERR, "setsockopt(SO_REUSEADDR) failed");
		return -1;
	}
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0) {
        syslog(LOG_ERR, "setsockopt(SO_REUSEPORT) failed");
        return -1;
    }
	if(bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
		syslog(LOG_ERR, "bind failed");
		return -1;
	}
	
	// Listens for and accepts a connection
	if(listen(sockfd, 10) < 0) {
		syslog(LOG_ERR, "listen failed");
		return -1;
	}

	struct sockaddr_storage their_addr;
	socklen_t addr_size = sizeof their_addr;
	int new_socket = accept(sockfd, (struct sockaddr*)&their_addr, &addr_size);
	if(new_socket < 0) {
		syslog(LOG_ERR, "accept failed");
		return -1;
	}

	// Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client.
	char client_ip[INET6_ADDRSTRLEN];
	
	if(inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), client_ip, sizeof client_ip) == NULL) {	
		syslog(LOG_ERR, "inet_ntop failed");
		return -1;
	}
	syslog(LOG_USER, "Accepted connection from %s", client_ip); 

	// todo Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist. 
	//Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.
	// You may assume the data stream does not include null characters (therefore can be processed using string handling functions).
	// You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.
	int fd = open("/var/tmp/aesdsocketdata", O_CREAT | O_RDWR | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);	
	if(fd < 0) {
		syslog(LOG_ERR, "error opening log file /var/tmp/aesdsocketdata");
		return -1;
	}
	
	char* writedata = "test data to write to the file.";
	
	if(write(fd, writedata, strlen(writedata)) != (ssize_t)strlen(writedata)) {
		syslog(LOG_ERR, "error writing data to file.");
		return -1;
	}

	// todo Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.
	//You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) will be less than the size of the root filesystem, however you may not assume this total size of all packets sent will be less than the size of the available RAM for the process heap.
	fdatasync(fd);	
	close(fd);

	// todo  Logs message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.
	close(new_socket);
	syslog(LOG_USER, "Closed connection from %s", client_ip);

	// todo Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received (see below).

	// todo  Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.
	// Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.	


	freeaddrinfo(servinfo);

	return 0;
}

