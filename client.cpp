#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#define BUFLEN 255


#define SUCCESS 100000
#define LOGIN_BRUTE_FORCE -8
#define ALREADY_LOGGED_IN -2


#define QUIT_CMD 10
#define DEFAULT_CMD 1

#define LOGOUT_INVALID_USER -1
#define LOGOUT_SUCCESSFUL 1001
#define GETUSERLIST_SUCCESSFUL 1002
#define GETUSERLIST_EMPTY 1003

/*
 * All about server socket
 */
int sockfd;
int client_file_fd;
struct sockaddr_in server_addr;

/*
 * Variables for local purposes
 */
int result;
char buffer[BUFLEN];

/*
 * For select call
 */
int fdmax;
fd_set original;
fd_set modified;


int get_command_code(char *command)
{
	char copy[BUFLEN];
	char *tok;
	memset(copy, 0, BUFLEN);
	memcpy(copy, command, BUFLEN);
	tok = strtok(copy, " \n");
	if (strcmp (tok, "quit") == 0) {
		return QUIT_CMD;
	}
	return DEFAULT_CMD;
}

bool check_buffer_empty(char *buffer)
{
	char copy[BUFLEN];
	char *tok;

	memset(copy, 0, BUFLEN);
	memcpy(copy, buffer, BUFLEN);
	tok = strtok(copy, " \n");
	if (tok == NULL)
		return true;
	return false;
}

void write_log(char *buff)
{
	write(client_file_fd, buff, strlen(buff));		
}

int main(int argc, char ** argv)
{
	if (argc <= 2){
		perror("./client <server IP> <server PORT> \n");
		exit(1);
	}


	/*
	 * Socket open
	 */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd <= 0) {
		perror ("Cannot open client socket");
		exit(1);
	}

	/*
	 * Setup the socket
	 */
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(argv[1]);
	server_addr.sin_port = htons(atoi(argv[2]));

	/*
	 * Connect to server
	 */ 

	int size = sizeof(server_addr);
	result = connect(sockfd, (struct sockaddr *) &server_addr, size);
	if (result < 0) {
		perror("Cannot connect to server ");
		exit(1);
	}

	/*
	 * Create the client file
	 */

	int pid = getpid();
	char name[BUFLEN];
	sprintf(name, "client-%d.log", pid);
	client_file_fd = open(name, O_CREAT | O_TRUNC | O_RDWR | O_APPEND , 0644);
	FILE * client_file = fdopen(client_file_fd, "rw");
	if (client_file == NULL)
		perror("Cannot create file");

	char buffer[BUFLEN];
	char prompt[BUFLEN];
	memset(buffer, 0 , sizeof(buffer));
	memset(prompt, 0 , sizeof(prompt));

	/*
	 * Do what select is meant to do
	 */
	FD_SET(STDIN_FILENO, &original);
	FD_SET(sockfd, &original);
	if (sockfd > fdmax)
		fdmax = sockfd;

	while(1) {

		modified = original;
		fputs(prompt, stdout);
		fputs(">", stdout);
		fflush(stdout);
		//fgets(buffer, BUFLEN, stdin);
		/*
		 * Listen for commands
		 */
		result = select(fdmax + 1, &modified, NULL, NULL, NULL);
		if (result < 0) {
			perror("Error in select \n");
			exit(0);
		}
		
		for (int i = 0; i <= fdmax; ++i) {
			/*
			 * If I need to read from stdin
			 */
			if (i == STDIN_FILENO) {
				read(STDIN_FILENO, buffer, BUFLEN);
				if (check_buffer_empty(buffer) || (strlen(buffer) <= 1)) {
					printf("Buffer is empty\n");
					memset(buffer, 0 , BUFLEN);
					continue;
				}
				/*
				 * Quit
				 */
				if (get_command_code(buffer) == QUIT_CMD) {
					close(sockfd);
					exit(0);
				}
				/*
				 * Send the command to server
				 */
				send(sockfd, buffer, BUFLEN, 0);
				/*
				 * Log the command
				 */
				write_log(buffer);
			}
			
			else if (i == sockfd) {
				/*
				 * If the buffer is empty, it means
				 * I have not send any information and that
				 * what I received from stdin was empty
				 */
				if (check_buffer_empty(buffer) || (strlen(buffer) <= 1)) {
					memset(buffer, 0 , BUFLEN);
					continue;
				}
				memset(buffer, 0, BUFLEN);
				result = recv(sockfd, buffer, BUFLEN, 0);
				printf("Received %d bytes\n", result);
				printf("Received = %s on %d\n", buffer, i);
				printf("Atoi(buffer) = %d\n", atoi(buffer));
				switch (atoi(buffer)) {
					case SUCCESS:
					{
						printf("Login successful \n");
						/*
						 * Get username from server for prompt
						 */
						memset(prompt, 0 , BUFLEN);
						recv(sockfd, prompt, BUFLEN, 0);
						char message[] = "Login successful\n";
						write_log(message);
						memset(buffer, 0, BUFLEN);
						break;
					}
					case LOGIN_BRUTE_FORCE:
					{
						printf("-8 Brute force detectat\n");
						char message[] = "-8 Brute force detectat\n";
						write_log(message);
						break;
					}
					case ALREADY_LOGGED_IN:
					{
						printf("-2 Sesiune deja deschisa\n");
						char message[] = "-2 Sesiune deja deschisa\n";
						write_log(message);
						memset(buffer, 0, BUFLEN);
						recv(sockfd, prompt, BUFLEN, 0);
						break;
					}
					case LOGOUT_INVALID_USER:
					{
						printf("-1 Clientul nu e autentificat");
						char message[] = "-1 Clientul nu e autentificat\n";
						write_log(message);
						memset(buffer, 0, BUFLEN);
						break;
					}
					case LOGOUT_SUCCESSFUL:
					{
						memset(prompt, 0, BUFLEN);
						memset(buffer, 0, BUFLEN);
						break;
					}
					case GETUSERLIST_SUCCESSFUL:
					{
						int N; //number of users
						/*
						 * Get the number of users
						 */
						memset(buffer, 0, BUFLEN);	
						recv(i, buffer, BUFLEN, 0);
						N = atoi(buffer);
						/*
						 * Receive the name of each user
						 */ 
						for (int j = 0; j < N; ++j) {
							recv(i, buffer, BUFLEN, 0);
							printf("%s\n",buffer);
							write_log(buffer);
							memset(buffer, 0, BUFLEN);
						}
						break;
					}
					case GETUSERLIST_EMPTY:
					{
						char message[] = "-1 Cientul nu e autentificat\n";
						printf("%s", message);
						write_log(message);
						break;
					}
					case DEFAULT_CMD:
					{
						break;
					}
					default:
						break;
				}
			}

			else {
				//result = read(sockfd, buffer, BUFLEN);
				if (result <= 0) {
					perror("Error when client receiving\n");
				}
				//printf("Received %s from %d \n", buffer, i);
			}
		}
	}

	fclose(client_file);
	close(client_file_fd);

	return 0;
}

