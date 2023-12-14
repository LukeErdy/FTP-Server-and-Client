#include "myftp.h"

void errCheck(int res) {
	if (res < 0) {
		fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
        exit(1);
	}
}

int sendErr(int res, int connectfd) {
	if (res < 0) {
		write(connectfd, "E", 1);
		char* str = strerror(errno);
		write(connectfd, str, strlen(str));
		write(connectfd, "\n", 1);
		return 0;
	}
	return 1;
}

int createCon(int port, int conType, int connectfd) { // Function adapted from assignment 8
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    errCheck(listenfd);
    errCheck(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)));
    struct sockaddr_in servAddr;
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_port = htons(port);
    if (conType) {
    	if (sendErr(bind(listenfd, (struct sockaddr*)&servAddr, sizeof(servAddr)), connectfd)) {
    		socklen_t addrlen = sizeof(servAddr);
	    	if (sendErr(getsockname(listenfd, (struct sockaddr*)&servAddr, (socklen_t*)&addrlen), connectfd)) {
	    		if (sendErr(listen(listenfd, 1), connectfd)) {
	    			write(connectfd, "A", 1);
			    	char num[6];
					sprintf(num, "%d", ntohs(servAddr.sin_port));
			    	write(connectfd, num, 5);
			    	write(connectfd, "\n", 1);
	    		} else return -1;
	    	} else return -1;
    	} else return -1;
    } else {
    	servAddr.sin_family = AF_INET;
    	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    	errCheck(bind(listenfd, (struct sockaddr*)&servAddr, sizeof(servAddr)));
    	errCheck(listen(listenfd, 4));
    }
    return listenfd;
}

int getDataCon(int connectfd) {
	int listenfd = createCon(0, 1, connectfd);
	if (listenfd != -1) {
		int length = sizeof(struct sockaddr_in);
    	struct sockaddr_in clientAddr;
		int datafd = accept(listenfd, (struct sockaddr*)&clientAddr, &length);
		if (sendErr(datafd, connectfd)) {
			return datafd;
		} else return 0;
	} else return 0;
}

void Ccom(int connectfd, char* buf) {
	buf[strcspn(buf, "\n")] = '\0';
	if (sendErr(chdir(&buf[1]), connectfd)) {
		write(connectfd, "A\n", 2);
		printf("Child %d: Changed current directory to %s\n", getpid(), &buf[1]);
	}
}

void Lcom(int connectfd, int datafd) {
	int pid = fork();
	if (sendErr(pid, connectfd)) {
		if (pid) {
			waitpid(-1, NULL, 0);
			write(connectfd, "A\n", 2);
			close(datafd);
		} else {
			if (sendErr(dup2(datafd, 1), connectfd)) {
				execlp("ls", "ls", "-l", NULL);
				sendErr(-1, connectfd);
			}
		}
	}
}

void Gcom(int connectfd, int datafd, char* buf) {
	buf[strcspn(buf, "\n")] = '\0';
	struct stat fileInfo;
	if (sendErr(lstat(&buf[1], &fileInfo), connectfd)) {
		if (S_ISREG(fileInfo.st_mode)) {
			if (sendErr(access(&buf[1], R_OK), connectfd)) {
				int filefd = open(&buf[1], O_RDONLY);
				printf("Child %d: Reading file %s\n", getpid(), &buf[1]);
				ssize_t bytesRead = 0;
				char* buffer = calloc(MAX_BUF_LEN, 1);
				write(connectfd, "A\n", 2);
				printf("Child %d: Transmitting file %s to client\n", getpid(), &buf[1]);
				while ((bytesRead = read(filefd, buffer, MAX_BUF_LEN)) > 0) {
					write(datafd, buffer, bytesRead);
					free(buffer);
					buffer = calloc(MAX_BUF_LEN, 1);
					bytesRead = 0;
				}
				free(buffer);
				close(filefd);
				close(datafd);
			}
		} else {
			write(connectfd, "E", 1);
			write(connectfd, "File is not regular.\n", 21);
		}
	}
}

void Pcom(int connectfd, int datafd, char* buf) {
	buf[strcspn(buf, "\n")] = '\0';
	int last = 0;
	int slashes = 0;
	for (int i = 1; i < strlen(buf); i++) {
		if (buf[i] == '/') {
			last = i;
			slashes++;
		}
	}
	if (access(&buf[last + 1], F_OK)) {
		int filefd = open(&buf[last + 1], O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IXUSR);
		printf("Child %d: Writing file %s\n", getpid(), &buf[last + 1]);
		ssize_t bytesRead = 0;
		char* buffer = calloc(MAX_BUF_LEN, 1);
		write(connectfd, "A\n", 2);
		printf("Child %d: Receiving file %s from client.\n", getpid(), &buf[last + 1]);
		while ((bytesRead = read(datafd, buffer, MAX_BUF_LEN)) > 0) {
			write(filefd, buffer, bytesRead);
			free(buffer);
			buffer = calloc(MAX_BUF_LEN, 1);
			bytesRead = 0;
		}
		free(buffer);
		close(filefd);
		close(datafd);
	} else {
		write(connectfd, "E", 1);
		write(connectfd, "File already exists.\n", 21);
	}
}

void Qcom(int connectfd) {
	write(connectfd, "A\n", 2);
	printf("Child %d: Quitting...\n", getpid());
	exit(0);
}

void readAndParse(int connectfd, int datafd) {
	char buf[MAX_BUF_LEN];
	int bytesRead;
	int totalRead = 0;
	while (totalRead < MAX_BUF_LEN) {
		bytesRead = read(connectfd, buf + totalRead, 1);
		if (buf[totalRead] == '\n') break;
		totalRead += bytesRead;
	}
	if (buf[0] == 'D') readAndParse(connectfd, getDataCon(connectfd));
	else if (buf[0] == 'C') Ccom(connectfd, buf);
	else if (buf[0] == 'Q') Qcom(connectfd);
	else if (datafd) {
		if (buf[0] == 'L') Lcom(connectfd, datafd);
		else if (buf[0] == 'G') Gcom(connectfd, datafd, buf);
		else if (buf[0] == 'P') Pcom(connectfd, datafd, buf);
	} else {
		write(connectfd, "E", 1);
		write(connectfd, "No data connection established.\n", 32);
	}
	readAndParse(connectfd, 0);
}

void connectLoop(int listenfd) {
	int connectfd;
    int length = sizeof(struct sockaddr_in);
    struct sockaddr_in clientAddr;
    int pid;
    while (1) {
    	connectfd = accept(listenfd, (struct sockaddr*)&clientAddr, &length);
    	errCheck(connectfd);
    	pid = fork();
    	if (pid) {
    		while (waitpid(-1, NULL, WNOHANG)) {
			if (errno == ECHILD) {
				errCheck(-1);
				break;
			}
    		}
    	} else {
    		char clientName[NI_MAXHOST];
    		int clientEntry;
    		if ((clientEntry = getnameinfo((struct sockaddr*)&clientAddr, sizeof(clientAddr), clientName, sizeof(clientName), NULL, 0, NI_NUMERICSERV)) != 0) {
		        fprintf(stderr, "Error: %s\n", gai_strerror(clientEntry));
		        exit(1);
		    }
		    printf("Child %d: Connection accepted from host %s\n", getpid(), clientName);
		    readAndParse(connectfd, 0);
    	}
    }
}

int main() {
	connectLoop(createCon(MY_PORT_NUMBER, 0, 0));
	return 0;
}
