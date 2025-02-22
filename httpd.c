 /*
 * must use scandir flag in Makefile:
 * CPPFLAGS=-MMD -MF $*.d -D_POSIX_C_SOURCE=200809L
 */

/*
 * TODO: Modularize big function
 * 	- have a very clear function call in main()
 * 	   - no nested for loops
 * 	   - no nested if statements
 *
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <fcntl.h>

// for scandir
#define _POSIX_C_SOURCE 200809L
#define SERVER_IP "127.0.0.1"
#define PORT 8080

// returns position of file in list of files, if present
// else return -1
int find_file(char * file_name, struct dirent **namelist, int num_files) {
	int i, found = 0; 
	char * curr_file;
	for (i = 0; i < num_files; i++) {
		curr_file = namelist[i]->d_name;
		if (strncmp(file_name, curr_file, strlen(curr_file)) == 0){
			printf("[*] Found file!\n");
			found = 1;
			break;
		}
	}
	if (found)
		return i;
	else
		return -1;
}

// writes the correct HTTP header and content into response
int create_http_response(
	char * response,
	char * file_name,
	struct dirent **namelist,
	int num_files, char * response_type
) {
	int fd, file_pos, offset;
	// max size of content is 1 MB
	unsigned char content[1000000]; 
	char html[10] = "", jpg[10] = "", gif[10] = "", ico[10] = "", *file;;

	file_pos = find_file(file_name, namelist, num_files);

	// if file cannot be found return 404 header
	if (file_pos < 1) {
		strncpy(response, "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\nConnection: close\nContent-Length: 5000\r\n\r\n<!doctype html><html><head><title>404 Not Found</title></head><body>404 - Not Found</body></html>\0", 5115);
		return 1;
	}

	strncpy(html, "html", 5);
	strncpy(jpg, "jpg", 5);
	strncpy(gif, "gif", 5);
	strncpy(ico, "ico", 5);

	int isJpg, isGif, isIco;

	isJpg = strncmp(response_type, jpg, strlen(jpg)) == 0;
	isGif = strncmp(response_type, gif, strlen(gif)) == 0;
	isIco = strncmp(response_type, ico, strlen(ico)) == 0;
	
	// case: html
	if (strncmp(response_type, html, strlen(html)) == 0) {
		printf("[*] It's an html file!\n");
		file = namelist[file_pos]->d_name;
		fd = open(file, O_RDWR);
		if (fd < 0) {
			fprintf(stderr, "[*] ERROR opening file\n");
			return 1;
		}

		if (read(fd, content, sizeof(content)) < 0) {
			fprintf(stderr, "[*] ERROR writing file\n");
			return 1;
		}
		// add end of file
		offset = lseek(fd, 0, SEEK_END);
		content[offset] = '\0';

		sprintf(response, "HTTP/1.0 200 OK\nContent-Type: text/html\nConnection: close\nContent-Length: %ld\n\n%s", sizeof(content), content);
	}

	// case: picture
	else if (isJpg  || isGif || isIco) {
		FILE * picture;
		char type[5] = "";
		if (isJpg)
			strncpy(type, "jpeg", 5);
		else if (isGif)
			strncpy(type, "gif", 5);
		else if (isIco)
			strncpy(type, "icon", 5);
		else {
			printf("[*] Error, no such type\n");
			exit(1);
		}

		printf("[*] It's a picture!\n");
		
		picture = fopen(file_name, "rb");
		if (picture == NULL) {
			fprintf(stderr, "[*] ERROR reading Image!\n");
		}
		printf("[*] Picture found!\n");

		int n = fread(content, sizeof(unsigned char), 1000000, picture);
		printf("[*] Number of bytes read: %d\n", n);
		fseek(picture, 0, SEEK_END);
		int m = ftell(picture);
		fseek(picture, 0, SEEK_SET);
		printf("[*] All of content successfully read: : %d\n", n == m);

		sprintf(response, "HTTP/1.0 200 OK\r\nContent-Type: text/%s\r\nConnection: close\r\nContent-Length: %d\r\n\r\n", type, n);
		printf("[*] String length of response: %ld\n",strlen(response));
		memcpy(&response[strlen(response)], content, n+1);
		response[n+2] = '\0';
	}

	return 0;

}

int main() {

	int sock, success, s2, len;
	unsigned int t;
	struct sockaddr_in local, remote;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		fprintf(stderr, "[*] ERROR creating socket\n");
		exit(1);
	}
	printf("[*] Socket creation successful.\n");

	local.sin_family = AF_INET;
	success = inet_pton(AF_INET, SERVER_IP, &local.sin_addr.s_addr);
	if (success <= 0) {
		if (success == 0)
			fprintf(stderr, "[*] Not in presenatio format");
		else
			perror("inet_pton");
		exit(EXIT_FAILURE);
	}
	// use http port
	local.sin_port = htons(PORT);

	len = sizeof(local);
	// bind to socket to a file descriptor
	success = bind(sock, (struct sockaddr *) &local, len);
	if (success == -1){
		fprintf (stderr, "[*] ERROR binding\n");
		exit(1);
	}
	printf("[*] Binding successful.\n");

	// increase 1 to n to accept more conections
	if (listen(sock, 7) == -1) {
		fprintf(stderr, "[*] ERROR listening\n");
		exit(1);
	}
	printf("[*] Listening successful.\n");

	while (1) {
		pid_t pid;
		printf("[*] Listening on port 8080.....\n");

		t = sizeof(remote);
		// open a new socket for the new connection
		s2 = accept(sock, (struct sockaddr *) &remote, &t);
		if (s2 == -1){
			fprintf(stderr, "[*] ERROR accepting\n");
			return 1;
		}
		printf("[*] Socket accepted: Connected\n");


		pid = fork();
		printf("[*] Forked!\n");
		// child process will take care of dealing with HTTP request
		if (pid == 0) {

			close(sock);
			int req;
			char browser_req[1048];

			// receive input from client
			req = read(s2, browser_req, 512);
			if (req == -1) {
				fprintf(stderr, "[*] Error receiving\n");
			}

			char *dir, response[1100000], file_name[100] = "",
			     response_type[10] = "";
			struct dirent **namelist;
			int num_files, c;

			response[0] = '\0';
			printf("[*] Browser request:\n %s\n", browser_req);
			// extract the file name
			sscanf(browser_req, "GET /%s HTTP/1", file_name);
			printf("[*] File name: %s\n", file_name);
			// extract the response type
			sscanf(browser_req, "GET /%*[^. ].%s", response_type);
			printf("[*] Response type: %s\n", response_type);


			dir = ".";
			// read the contents of directory list into namelist, get number of files in directory
			// TODO: is it prettier to call this in each function and have local copies of contents of the directory for all functions, or to pass this all the time?
			num_files = scandir(dir, &namelist, NULL, alphasort);
			if (num_files < 0) {
				fprintf(stderr, "[*] Scandir Error: directory does not exist\n");
				close(s2);
				close(sock);
				exit(1);
			}


			c = create_http_response(response, file_name, namelist, num_files, response_type);
			if (c) {
				fprintf(stderr, "[*] Error opening and reading file\n");
				if (write(s2, response, sizeof(response)) < 0) {
					fprintf(stderr, "[*] ERROR sending\n");
				}
				printf("[*] 404 sent, closing sockets\n");
				close(s2);
				close(sock);
				exit(1);
			}
			printf("[*] Constructed response\n");
			printf("[*] Response length: %ld\n", strlen(response));


			if (write(s2, response, sizeof(response)) < 0) {
				fprintf(stderr, "[*] ERROR sending\n");
			}
			printf("[*] Content sent\n");

			printf("[*] Closing socket\n\n");
			close (s2);
			close(sock);
			exit(0);
			
		}
		else{
			continue;
		}
	}
	close(sock);

	return 0;
}

