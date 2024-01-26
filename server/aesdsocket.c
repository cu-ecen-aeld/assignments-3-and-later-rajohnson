#include <syslog.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <iso646.h>
#include <sys/socket.h>
#include <sys/types.h>

int main(int argc, char **argv) {
	printf("aesdsocket\n");	

	openlog("writer", 0, LOG_USER);

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
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
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
	// todo - still need to call getaddrinfo and then bind.
	
	
	// todo Listens for and accepts a connection

	// todo Logs message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client.

	// todo Receives data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist. 
	//Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.
	// You may assume the data stream does not include null characters (therefore can be processed using string handling functions).
	// You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.

	// todo Returns the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.
	//You may assume the total size of all packets sent (and therefore size of /var/tmp/aesdsocketdata) will be less than the size of the root filesystem, however you may not assume this total size of all packets sent will be less than the size of the available RAM for the process heap.

	// todo  Logs message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.

	// todo Restarts accepting connections from new clients forever in a loop until SIGINT or SIGTERM is received (see below).

	// todo  Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.
	// Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.	

/* old program
	char message[1500] = {0};
	char str[500];
	char filename[500];

	strncpy(str, argv[2], sizeof(str));
	strncpy(filename, argv[1], sizeof(filename));

	snprintf(message, 1500, "writing %s to %s", str, filename);

	syslog(LOG_DEBUG, "%s", message);

	// open file
	FILE * fp = fopen(filename, "w"); // w option will truncate existing files
	if(fp == NULL) {
		syslog(LOG_ERR, "%s", "File could not be opened");
		return 1;
	}

	// write string to file
	if(fwrite(str, 1, strlen(str), fp) != strlen(str)) {
		syslog(LOG_ERR, "%s", "Could not write to file");
		return 1;
	}

	// close file
	fclose(fp);
*/

	return 0;
}

