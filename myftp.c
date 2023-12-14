#include "myftp.h"

void errCheck(int res) {
	if (res < 0) {
		fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
        exit(1);
	}
}

int createCon(const char* address, char* port) { // Function adapted from assignment 8
	int socketfd;
    struct addrinfo hints, *actualdata;
    memset(&hints, 0, sizeof(hints));
    int err;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    if ((err = getaddrinfo(address, port, &hints, &actualdata)) != 0) {
        fprintf(stderr, "Error: %s\n", gai_strerror(err));
        exit(1);
    }
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    errCheck(socketfd);
    errCheck(connect(socketfd, actualdata->ai_addr, actualdata->ai_addrlen));
    return socketfd;
}

char* getServerRes(int socketfd) {
	char* res = calloc(MAX_BUF_LEN, sizeof(char));
	int bytesRead;
	int totalRead = 0;
	while (totalRead < MAX_BUF_LEN) {
		bytesRead = read(socketfd, res + totalRead, 1);
		if (res[totalRead] == '\n') break;
		totalRead += bytesRead;
	}
	res[strcspn(res, "\n")] = '\0';
	if (res[0] == 'A') {
		return &res[1];
	}
	fprintf(stderr, "Error: %s\n", &res[1]);
	free(res);
	return NULL;
}

int pathExists(char* pathName) {
	if (!pathName) {
		fprintf(stderr, "Command error: expecting a parameter.\n");
		return 0;
	}
	return 1;
}

void more(int datafd) {
	int pid = fork();
	if (pid == -1) fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
	else {
		if (pid) waitpid(-1, NULL, 0);
		else {
			if (dup2(datafd, 0) == -1) fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
			else {
				execlp("more", "more", "-20", NULL);
				fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
			}
		}
	}
}

void exitCom(int socketfd) {
	write(socketfd, "Q\n", 2);
	char* res = getServerRes(socketfd);
	if (res) free(&res[-1]);
	exit(0);
}

void cdCom(char* pathName) {
	if (pathExists(pathName)) {
 		if (chdir(pathName)) fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
 	}
}

void rcdCom(int socketfd, char* pathName) {
	if (pathExists(pathName)) {
		write(socketfd, "C", 1);
		write(socketfd, pathName, strlen(pathName));
		write(socketfd, "\n", 1);
		char* res = getServerRes(socketfd);
		if (res) free(&res[-1]);
	}
}

void lsCom() {
	int pid0 = fork();
	if (pid0 == -1) fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
	else {
		if (pid0) waitpid(-1, NULL, 0);
		else {
			int pipefd[2];
			if (pipe(pipefd) == -1) fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
			else {
				int pid1 = fork();
				if (pid1 == -1) fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
				else {
					if (pid1) {
						close(pipefd[1]);
						dup2(pipefd[0], 0);
						close(pipefd[0]);
						waitpid(-1, NULL, 0);
						execlp("more", "more", "-20", NULL);
						fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
					} else {
						close(pipefd[0]);
						dup2(pipefd[1], 1);
						close(pipefd[1]);
						execlp("ls", "ls", "-l", NULL);
						fprintf(stderr, "Error: %s (%d)\n", strerror(errno), errno);
					}
				}
			}
		}
	}	
}

void rlsCom(const char* address, int socketfd) {
	write(socketfd, "D\n", 2);
	char* res = getServerRes(socketfd);
	if (res) {
		int datafd = createCon(address, res);
		free(&res[-1]);
		write(socketfd, "L\n", 2);
		res = getServerRes(socketfd);
		if (res) {
			free(&res[-1]);
			more(datafd);
		}
		close(datafd);
	}
}

void getCom(const char* address, int socketfd, char* pathName) {
	if (pathExists(pathName)) {
		int last = 0;
		int slashes = 0;
		for (int i = 0; i < strlen(pathName); i++) {
			if (pathName[i] == '/') {
				last = i;
				slashes++;
			}
		}
		if ((slashes && access(&pathName[last + 1], F_OK)) || (!slashes && access(pathName, F_OK))) {
			write(socketfd, "D\n", 2);
			char* res = getServerRes(socketfd);
			if (res) {
				int datafd = createCon(address, res);
				free(&res[-1]);
				write(socketfd, "G", 1);
				write(socketfd, pathName, strlen(pathName));
				write(socketfd, "\n", 1);
				res = getServerRes(socketfd);
				if (res) {
					free(&res[-1]);
					int filefd;
					if (slashes) filefd = open(&pathName[last + 1], O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IXUSR);
					else if (!slashes) filefd = open(pathName, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IXUSR);
					ssize_t bytesRead = 0;
					char* buffer = calloc(MAX_BUF_LEN, 1);
					while ((bytesRead = read(datafd, buffer, MAX_BUF_LEN)) > 0) {
						write(filefd, buffer, bytesRead);
						free(buffer);
						buffer = calloc(MAX_BUF_LEN, 1);
						bytesRead = 0;
					}
					free(buffer);
					close(filefd);
				}
				close(datafd);
			}
		} else fprintf(stderr, "Error: File exists locally.\n");
	}
}

void showCom(const char* address, int socketfd, char* pathName) {
	if (pathExists(pathName)) {
		write(socketfd, "D\n", 2);
		char* res = getServerRes(socketfd);
		if (res) {
			int datafd = createCon(address, res);
			free(&res[-1]);
			write(socketfd, "G", 1);
			write(socketfd, pathName, strlen(pathName));
			write(socketfd, "\n", 1);
			res = getServerRes(socketfd);
			if (res) {
				free(&res[-1]);
				more(datafd);
			}
			close(datafd);
		}
	}
}

void putCom(const char* address, int socketfd, char* pathName) {
	if (pathExists(pathName)) {
		struct stat fileInfo;
		if (!lstat(pathName, &fileInfo)) {
			if (S_ISREG(fileInfo.st_mode)) {
				if (!access(pathName, R_OK)) {
					write(socketfd, "D\n", 2);
					char* res = getServerRes(socketfd);
					if (res) {
						int datafd = createCon(address, res);
						free(&res[-1]);
						write(socketfd, "P", 1);
						write(socketfd, pathName, strlen(pathName));
						write(socketfd, "\n", 1);
						res = getServerRes(socketfd);
						if (res) {
							free(&res[-1]);
							int filefd = open(pathName, O_RDONLY);
							ssize_t bytesRead = 0;
							char* buffer = calloc(MAX_BUF_LEN, 1);
							while ((bytesRead = read(filefd, buffer, MAX_BUF_LEN)) > 0) {
								write(datafd, buffer, bytesRead);
								free(buffer);
								buffer = calloc(MAX_BUF_LEN, 1);
								bytesRead = 0;
							}
							free(buffer);
							close(filefd);
						}
						close(datafd);
					}
				} else fprintf(stderr, "Error: File is not readable.\n");
			} else fprintf(stderr, "Error: File is not regular.\n");
		} else fprintf(stderr, "Error: File does not exist locally.\n");
	}
}

void parseInput(const char* address, int socketfd, char* com, char* pathName) {
	if (!strcmp(com, "exit")) exitCom(socketfd);
	else if (!strcmp(com, "cd")) cdCom(pathName);
	else if (!strcmp(com, "rcd")) rcdCom(socketfd, pathName);
	else if (!strcmp(com, "ls")) lsCom();
	else if (!strcmp(com, "rls")) rlsCom(address, socketfd);
	else if (!strcmp(com, "get")) getCom(address, socketfd, pathName);
	else if (!strcmp(com, "show")) showCom(address, socketfd, pathName);
	else if (!strcmp(com, "put")) putCom(address, socketfd, pathName);
	else fprintf(stderr, "Command '%s' is unknown - ignored\n", com);	
}

void commandLoop(const char* address, int socketfd) {
	printf("Connected to server %s\n", address);
	while (1) {
		char input[MAX_BUF_LEN];
		printf("MYFTP> ");
		fgets(input, MAX_BUF_LEN, stdin);
		char* com = strtok(input, " \t\n");
		char* pathName = strtok(NULL, " \t\n");
		if (com) parseInput(address, socketfd, com, pathName);
	}
}

int main(int argc, char const *argv[]) {
	if (argc != 2) {
		fprintf(stdout, "Usage: ./myftp <hostname | IP address>\n");
		exit(1);
	}
	char port[6];
	sprintf(port, "%d", MY_PORT_NUMBER);
	commandLoop(argv[1], createCon(argv[1], port));
	return 0;
}
