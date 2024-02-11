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
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/queue.h>
#include <pthread.h>

#define NUM_CONNECTIONS (10)

struct slist_data_s {
	int thread_handle;
	SLIST_ENTRY(slist_data_s) entries;
};

SLIST_HEAD(slisthead, slist_data_s) head = SLIST_HEAD_INITIALIZER(head);
struct slist_data_t *datap = NULL;

pthread_mutex_t file_mutex;

int server_fd; // file descriptor for the server socket

// get sockaddr, IPv4 or IPv6 -- from Beej's guide
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void signal_handler(int signal) {
	(void)signal; // silence compiler warning about unused variable, needs this function signature for signal
	// Gracefully exits when SIGINT or SIGTERM is received, completing any open connection operations, closing any open sockets, and deleting the file /var/tmp/aesdsocketdata.
	// Logs message to the syslog “Caught signal, exiting” when SIGINT or SIGTERM is received.	
	syslog(LOG_USER, "Caught signal, exiting");
	close(server_fd);
	if(remove("/var/tmp/aesdsocketdata") != 0) {
		syslog(LOG_ERR, "error deleting /var/tmp/aesdsocketdata");
	}

	while(not SLIST_EMPTY(&head)) {
		int thread_id = SLIST_FIRST(&head)->thread_handle;
		struct slist_data_s* entry = SLIST_FIRST(&head);
		SLIST_REMOVE_HEAD(&head, entries);
		free(entry);
		printf("%i", thread_id);
		// todo - join threads
	}

	exit(EXIT_SUCCESS);
}

void connection_handler(int client_fd, struct sockaddr_storage their_addr) {
	// Log message to the syslog “Accepted connection from xxx” where XXXX is the IP address of the connected client.
	char client_ip[INET6_ADDRSTRLEN];

	if(inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr*)&their_addr), client_ip, sizeof client_ip) == NULL) {	
		syslog(LOG_ERR, "inet_ntop failed");
		exit(-1);
	}

	syslog(LOG_USER, "Accepted connection from %s", client_ip); 

	pthread_mutex_lock(&file_mutex);

	// Receive data over the connection and appends to file /var/tmp/aesdsocketdata, creating this file if it doesn’t exist. 
	//Your implementation should use a newline to separate data packets received.  In other words a packet is considered complete when a newline character is found in the input receive stream, and each newline should result in an append to the /var/tmp/aesdsocketdata file.
	// You may assume the data stream does not include null characters (therefore can be processed using string handling functions).
	// You may assume the length of the packet will be shorter than the available heap size.  In other words, as long as you handle malloc() associated failures with error messages you may discard associated over-length packets.
	int rxdata_fd = open("/var/tmp/aesdsocketdata", O_CREAT | O_RDWR | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);	
	if(rxdata_fd < 0) {
		syslog(LOG_ERR, "error opening log file /var/tmp/aesdsocketdata");
		exit(-1);
	}
	
	const int BUF_LEN = 100;
	char rx_data[BUF_LEN];
	memset(rx_data, 0, BUF_LEN);
	int numbytes;
	while((numbytes = recv(client_fd, rx_data, BUF_LEN - 1, 0)) > 0) {
		syslog(LOG_USER, "recieved[%li, %i]: %s", strlen(rx_data), numbytes, rx_data);
		if(write(rxdata_fd, rx_data, strlen(rx_data)) != (ssize_t)strlen(rx_data)) {
			syslog(LOG_ERR, "error writing data to file.");
			exit(-1);
		}

		if(rx_data[numbytes - 1] == '\n') {
			break;
		}

		memset(rx_data, 0, BUF_LEN);
	}

	pthread_mutex_unlock(&file_mutex);

	// Return the full content of /var/tmp/aesdsocketdata to the client as soon as the received data packet completes.
	// move back to the beginning of the file
	lseek(rxdata_fd, 0, SEEK_SET);

	char tx_data[BUF_LEN];
	while((numbytes = read(rxdata_fd, tx_data, BUF_LEN)) > 0) {
		send(client_fd, tx_data, numbytes, 0);
	}	

	fdatasync(rxdata_fd);	
	close(rxdata_fd);

	// Log message to the syslog “Closed connection from XXX” where XXX is the IP address of the connected client.
	close(client_fd);
	syslog(LOG_USER, "Closed connection from %s", client_ip);

	exit(0);
}


int main(int argc, char **argv) {
	bool is_daemon = false;
	openlog("aesdsocket", 0, LOG_USER);

	// check that the arguments exist
	if(argc > 2) {
		syslog(LOG_ERR, "too many arguments - usage: %s [-d]", argv[0]);
		return 1;
	}

	if(argc == 2) {
		if(strncmp(argv[1], "-d", 3) == 0) {
			is_daemon = true;
		} else {
			syslog(LOG_ERR, "invalid argument - usage: %s [-d]", argv[0]);
			return 1;
		}
	}

	// Set up linked list to track threads
	SLIST_INIT(&head);

	// Open a stream socket bound to port 9000, failing and returning -1 if any of the socket connection steps fail.
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
	server_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
	if(server_fd == -1) {
		syslog(LOG_ERR, "error opening socket");
        return -1;
	}
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    	syslog(LOG_ERR, "setsockopt(SO_REUSEADDR) failed");
		return -1;
	}
	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0) {
        syslog(LOG_ERR, "setsockopt(SO_REUSEPORT) failed");
        return -1;
    }
	if(bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
		syslog(LOG_ERR, "bind failed");
		return -1;
	}
	freeaddrinfo(servinfo);

	
	if(is_daemon) {
		pid_t pid = fork();

		if(pid == -1) { // error forking
			syslog(LOG_ERR, "error when calling fork()");
			return -1;
		} else if(pid != 0) {  // parent thread
			exit(EXIT_SUCCESS);
		}

		if(setsid() == -1) {
			syslog(LOG_ERR, "error calling setsid()");
			return -1;
		}

		if(chdir("/") == -1) {
			syslog(LOG_ERR, "error calling chdir");
			return -1;
		}

		// redirect stdin, stdout, stderr to /dev/null
		open("/dev/null", O_RDWR); // stdin
		dup(0); // stdout
		dup(0); // stderr
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);	

	// Listen for and accept a connection
	if(listen(server_fd, NUM_CONNECTIONS) < 0) {
		syslog(LOG_ERR, "listen failed");
		return -1;
	}

	struct sockaddr_storage their_addr;
	socklen_t addr_size = sizeof their_addr;

	// accept connections from new clients forever in a loop until SIGINT or SIGTERM is received.
	while(true) {
		int new_socket = accept(server_fd, (struct sockaddr*)&their_addr, &addr_size);
		if(new_socket < 0) {
			syslog(LOG_ERR, "accept failed");
			return -1;
		}

		// fork and have the child handle the connection
		pid_t pid = fork();
		if(pid == -1) {	
            syslog(LOG_ERR, "fork failed");
            return -1;
        } else if(pid == 0) { // child process
			connection_handler(new_socket, their_addr);
		} else { // parent process
			struct slist_data_s* thread_entry = malloc(sizeof(struct slist_data_s));
			if(thread_entry == NULL) {
				syslog(LOG_ERR, "Error creating thread handle linked list.");
				return -1;
			}

			thread_entry->thread_handle = pid;
			SLIST_INSERT_HEAD(&head, thread_entry, entries);
		}
	}

	return 0;
}

