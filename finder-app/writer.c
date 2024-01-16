#include <syslog.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	openlog("writer", 0, LOG_USER);

	// check that the arguments exist
	if(argc != 3) {
		syslog(LOG_ERR, "usage: writer filename string");
		return 1;
	}	

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

	return 0;
}

