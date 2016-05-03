#include <iostream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


/*
 * Decoding instructions
 */
#define LOGIN_CMD 10
#define DEFAULT_CMD 1
#define LOGIN_BRUTE_FORCE 2
#define QUIT_CMD 3
#define LOGOUT_CMD 4
#define GET_USER_LIST_CMD 5

/*
 * Responses
 */
#define SUCCESS 100000
#define USER_OR_PASS_WRONG -3
#define ALREADY_LOGGED_IN -2

#define LOGOUT_INVALID_USER -1
#define LOGOUT_SUCCESSFUL 1001
#define GETUSERLIST_SUCCESSFUL 1002
#define GETUSERLIST_EMPTY 1003

/*
 * Globals
 */
#define BUFLEN 255
#define MAX_USERS 200

/*
 * Erros
 */
#define FILE_NOT_FOUND -20

using namespace std;

typedef struct login_params {
	char username[BUFLEN];
	char password[BUFLEN];
} login_params_t;

typedef struct file {
	char filename[BUFLEN];
	long size;
	bool shared;
} file_t;

typedef struct user{
	int fd;
	int shared_files_no;
	char *username;
	file_t **files;
} user_t;

int server_sock;
int client_sock;
struct sockaddr_in server_addr;
struct sockaddr_in client_addr;

/*
 * Result for any opperation for debug purposes
 */
int result;

/*
 * FD_SETS
 */ 
 int fdmax;
 fd_set original;
 fd_set modified;


 FILE *user_file; 
 FILE *shared_file;


 /*
  * Login attempt monitor
  */
int login_attempt;

user_t **users;

int get_command_code(char *command)
{
	if (strcmp(command, "login") == 0)
		return LOGIN_CMD;
	else if (strcmp(command, "quit") == 0)
		return QUIT_CMD;
	else if (strcmp(command, "logout") == 0)
		return LOGOUT_CMD;
	else if (strcmp(command, "getuserlist") == 0)
		return GET_USER_LIST_CMD;

	return DEFAULT_CMD;
}

int login(login_params_t *params)
{
	int N; //lines in user_file
	char line[BUFLEN];
	char user[BUFLEN];//read user from file
	char pass[BUFLEN];//read pass from file
	memset(user, 0, BUFLEN);
	memset(pass, 0, BUFLEN);

	fseek(user_file, 0, SEEK_SET);

	fscanf(user_file, "%d", &N);
	for (int i = 0; i < N; ++i) {
		fgets(line, BUFLEN, user_file);
		fscanf(user_file, "%s %s", user, pass);
		printf("user = %s pass = %s\n", user, pass);
		if (strcmp(user, params->username) == 0
		 && strcmp(pass, params->password) == 0) {
			fseek(user_file, 0, SEEK_SET);
		 	return SUCCESS;
		 }
	}
	login_attempt++;
	fseek(user_file, 0, SEEK_SET);
	if (login_attempt >= 3)
		return LOGIN_BRUTE_FORCE;
	return USER_OR_PASS_WRONG;
}

int logout(int user_connection)
{
	/*
	 * If the user has not been authenticated
	 * it means he does not exist in the users list
	 */
	if (users[user_connection] == NULL)
		return LOGOUT_INVALID_USER;
	user_t *user = users[user_connection];
	/*
	 * The user exists and we must log him out
	 * which means deleting the reference;
	 */
	 free(user->username);
	 user->username = NULL;
	 free(user);
	 user = NULL;
	 users[user_connection] = NULL;
	 return LOGOUT_SUCCESSFUL;

}


void send_client_code(int fd, int code)
{
	char buf[BUFLEN];
	memset(buf, 0, BUFLEN);
	sprintf(buf, "%d", code);
	printf("Sending %d code %s\n", fd, buf);
	send(fd, buf, BUFLEN, 0);
}

void send_client_message(int fd, char *message)
{
	char buf[BUFLEN];
	memset(buf, 0, BUFLEN);
	memcpy(buf, message, BUFLEN);
	send(fd, buf, BUFLEN, 0);
}

/*
 * Get the list of users from the file
 */
void get_users_from_file(user_t **out)
{
	int N = 0;
	int curr = 0;
	char dummy_pass[BUFLEN];
	memset(dummy_pass, 0, BUFLEN);
	/*
	 * Position cursor at the beginning of the file
	 * (Others calls might have moved the cursor)
	 */
	fseek(user_file, 0, SEEK_SET);
	/*
	 * Get the number of users allowed and
	 * alloc an array of size <number_of_users>
	 */
	fscanf(user_file, "%d", &N);
	printf("read N = %d\n", N);
	/*
	 * Read the users. (just the first word of each row)
	 */
	 for (int i = 0; i < N; ++i) {
		out[curr] = (user_t *)malloc(1 * sizeof(user_t));
		out[curr]->username = (char *) malloc(BUFLEN * sizeof(char));
		fscanf(user_file, "%s %s", out[curr]->username, dummy_pass);
		printf("Username got is %s\n", out[curr]->username);
		curr++;
	 }
	 printf("Finished reading users from file \n");
}

int get_users_in_order(user_t **users_from_file, user_t ** users, user_t **output_users){

	int N = 0; //number of users
	int curr = 0; //current found users

	fseek(user_file, 0, SEEK_SET);
	fscanf(user_file, "%d", &N);
	for (int i = 0; i < N; ++i) {
		user_t *file_user = users_from_file[i];
		printf("Working with user %s\n", file_user->username);
		for (int j = 0; j < MAX_USERS; ++j) {

			/*
			 * There is no user authenticated
			 */
			if(users == NULL)
			{
				return curr;
			}
			/*
			 * No user has logged in on the j fd.
			 * Remember that users is an array that contains
			 * on the fd position, a structure user_t containing
			 * the username of the username connected on the fd socket id
			 */
			if (users[j] == NULL){
				continue;
			}

			/*
			 * Maybe there has been a deallocation of the username,
			 * but the user may still be referenced by the "array"
			 * thus users[j] != NULL and users[j]->username == NULL
			 */
			if(users[j]->username == NULL){
				continue;
			}

			if (strcmp(file_user->username, users[j]->username) == 0) {
						printf("on fd = %d user = %s\n",j, file_user->username);
					/*
					 * Alloc memory when we have users to add
					 * else the output remains empty meaning that
					 * no user exists in the list of users and this means
					 * no user has authenticated
					 */
					if (output_users == NULL)
						*output_users = (user_t *) malloc (MAX_USERS * sizeof(user_t));
					/*
					 * Add the new user to the ordered list of authenticated users
					 */
					if (output_users[curr] == NULL)
						output_users[curr] = (user_t *) malloc (1 * sizeof(user_t));
					if (output_users[curr]->username == NULL)
						output_users[curr]->username = (char *)malloc(BUFLEN * sizeof(char));
					memcpy(output_users[curr]->username, users[j]->username, BUFLEN);
					curr++;
			}
		}
	}
	return curr;
}

int get_users_from_file_count()
{
	int N;
	fseek(user_file, 0, SEEK_SET);
	fscanf(user_file, "%d", &N);
	return N;
}

void create_user_directories()
{
	int N = get_users_from_file_count();
	user_t **users = (user_t **)malloc(N * sizeof(user_t*));
	get_users_from_file(users);
	for (int i = 0; i < N; ++i) {
		mkdir(users[i]->username, 0777);
		if (errno == EEXIST){
			printf("Directory %s already exists\n", users[i]->username);
			continue;
		}
		if (errno == EACCES){
			printf("You do not have access to create %s\n", users[i]->username);
			continue;
		}
	}
}

int get_file_size(char *folder, char *file)
{

	char *tok;
	char prev_folder[BUFLEN];
	getcwd(prev_folder, BUFLEN);
	chdir(folder);

	struct stat data;
	memset(&data, 0, sizeof(data));

	char test[BUFLEN];
	memset(test, 0, BUFLEN);
	memcpy(test, file, BUFLEN);

	tok = strtok(test, "\n");
	printf("Filename is %s\n", tok); 
	int fd = open(tok, O_RDONLY);
	printf("FD = %d\n", fd);
	if (fd < 0)
		return FILE_NOT_FOUND;
	fstat(fd, &data);
	close(fd);
	chdir(prev_folder);
	return data.st_size;
}

/*
 * Add shared files for user
 * from the shared_files file
 */
void add_shared_files(user_t *user) {
	
	if (user->files == NULL)
		user->files = (file_t **)malloc(BUFLEN * sizeof(file_t*));
	
	int M = 0;
	int file_size = 0;
	fscanf(shared_file, "%d", &M);
	char line[BUFLEN];
	memset(line, 0, BUFLEN);
	char *tok;
	/*
	 * Jump over the first line in the file
	 * Parse the lines with username and file
	 */
	fgets(line, BUFLEN, shared_file);

	for (int i = 0; i < M; ++i) {
		fgets(line, BUFLEN, shared_file);	
		tok = strtok(line, ":");
		if (strcmp(user->username, tok) == 0) {
			tok = strtok(NULL, ":");
			/*
			 * Check if the file we want to add has been
			 * allocated
			 */
			int num_files = user->shared_files_no;
			if (user->files[num_files] == NULL)// the last file in the list of files
				user->files[num_files] = (file_t *)malloc(1 * sizeof(file_t));

			user->files[user->shared_files_no]->shared = true;
			memcpy(user->files[user->shared_files_no]->filename, tok, strlen(tok));
			user->shared_files_no++;
			/*
			 * Store the size of the files also
			 * The get_file_size function returns 
			 * FILE_NOT_FOUND if the file hasn't been found
			 * but I do not treat this exeption because
			 * the files are supposed to be there
			 */ 
		 	file_size = get_file_size(user->username, tok);
			user->files[num_files]->size = file_size;
			printf("Shared file for %s is %s with size %d\n", user->username, tok, file_size);
		}
	}
	fseek(shared_file, 0, SEEK_SET);
}

void add_private_file(user_t *user, char *filename) {
}

int main(int argc, char ** argv)
{
	if (argc <= 3){
		perror("./server <port> <user_file> <shared_file>");
		exit(1);
	}
	
	/*
	 * Open files
	 */
	user_file = fopen(argv[2], "r");
	if (user_file == NULL)
		perror("Cannot open user_file");
	shared_file = fopen(argv[3],"r");
	if(shared_file == NULL)
		perror("Cannot open shared_file");
	
	/*
	 * Create user files
	 */
	create_user_directories();	

	/*
	 * Open socket
	 */
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == 0) {
		perror("Cannot open first socket \n");
		exit(1);
	}


	/*
	 * Setup the address and port for server to listen on
	 */
	 memset((char *) &server_addr, 0, sizeof(server_addr));
	 server_addr.sin_family = AF_INET;
	 server_addr.sin_addr.s_addr = INADDR_ANY;
	 server_addr.sin_port = htons (atoi(argv[1]));
	
	/*
	 * Bind socket to address and  port
	 */

	result = bind(server_sock, (struct sockaddr *) &server_addr, sizeof(server_addr));
	if (result < 0) {
		perror( "Cannot bind socket to address \n");
		exit(1);
	}
	
	/*
	 * Call listen call
	 */
	listen(server_sock, MAX_USERS);


	/*
	 * Do the select statement
	 */
 	FD_ZERO(&original);
 	FD_ZERO(&modified);

 	if(server_sock > fdmax)
		fdmax = server_sock;
	
	FD_SET(fdmax, &original);
	FD_SET(STDIN_FILENO, &original);


	/*
	 * Listen for incoming connections
	 */
	while (1) {
		modified = original;
		if (select(fdmax + 1, &modified, NULL, NULL, NULL) == -1) 
			perror("ERROR in select");
	
		int i;
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &modified)) {
			
				if (i == server_sock) {
					// a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
					// actiunea serverului: accept()
					int clilen = sizeof(client_addr);
					int newsockfd;
					if ((newsockfd = accept(server_sock, (struct sockaddr *)&client_addr,(unsigned int *) &clilen)) == -1) {
						perror("ERROR in accept");
					} 
					else {
						//adaug noul socket intors de accept() la multimea descriptorilor de citire
						FD_SET(newsockfd, &original);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
					}
					printf("Noua conexiune la server \n");
				}

				else if (i == STDIN_FILENO) {
					char buffer[BUFLEN];
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN, stdin);
					if (strcmp(buffer, ""))
						continue;
					char *tok = strtok(buffer, " \n");
					result = get_command_code(tok);
					switch (result) {
						case QUIT_CMD:
						{
							close (server_sock);
							exit(0);
							break;
						}
						default:
							break;
					}
					printf("Command %s came \n", buffer);
				}
					
				else {
					// am primit date pe unul din socketii cu care vorbesc cu clientii
					//actiunea serverului: recv()
					char buffer[BUFLEN];
					memset(buffer, 0, BUFLEN);
					if ((result = recv(i, buffer, sizeof(buffer), 0)) <= 0) {
						if (result == 0) {
							//conexiunea s-a inchis
							printf("selectserver: socket %d hung up\n", i);
						} else {
							perror("ERROR in recv");
						}
						close(i); 
						FD_CLR(i, &original); // scoatem din multimea de citire socketul pe care 
					} 
					
					else { //recv intoarce >0
						printf ("\nAm primit de la clientul de pe socketul %d, mesajul: %s", i, buffer);
						char *command = strtok(buffer, " \n");
						printf("command = %s\n", command);
						int code = get_command_code(command);
						switch (code) {
							case LOGIN_CMD:
							{
								login_params_t params;
								memset(&params, 0, sizeof(login_params_t));
								char *tok = strtok(NULL, " \n");
								memcpy(params.username, tok, strlen(tok));
								tok = strtok(NULL, " \n");
								memcpy(params.password, tok, strlen(tok));
								result = login(&params);
								switch (result) {
									case SUCCESS:
									{	
										if (users != NULL && users[i] != NULL) {
											send_client_code(i, ALREADY_LOGGED_IN); 
											printf("Sent -2 already authenticated\n");
											send(i, users[i]->username, BUFLEN, 0);
											break;
										} else {
											send_client_code(i, SUCCESS);
											if (users == NULL)
												users = (user_t **)calloc(MAX_USERS, sizeof(user_t*));

											user_t *new_user = (user_t *)malloc(1 * sizeof(user_t));
											new_user->username = (char *)malloc(BUFLEN *sizeof(char));
											new_user->fd = i;
											new_user->shared_files_no = 0;
											new_user->files = NULL;
											memcpy(new_user->username, params.username, BUFLEN);
											users[i] = new_user;
											add_shared_files(users[i]);
											printf("New user->fd = %d\n", new_user->fd);
											printf("New user->username = %s\n", new_user->username);
											send(i, params.username, BUFLEN, 0);
											printf("sending %d message %s\n",i , params.username);
											printf("Login successful \n");
											break;
										}
										break;
									}

									case USER_OR_PASS_WRONG:
									{
										printf("-3 User/parola gresita \n");
										send_client_code(i, -3);
										break;
									}

									case LOGIN_BRUTE_FORCE:
									{
										printf("-8 Brute force detectat\n");
										send_client_code(i, -8);
										break;
									}
									default:
										break;
								}
								break;
							} //endcase LOGIN_CMD;
							case LOGOUT_CMD:
							{
								/*
								 *
								 */
								 result = logout(i);
								 switch(result) {
									case LOGOUT_INVALID_USER:
									{
										printf("-1 Clientul nu este autentificat");
										send_client_code(i, -1);
										break;
									}
									case LOGOUT_SUCCESSFUL:
									{
										printf("Logout successfull\n");
										send_client_code(i, LOGOUT_SUCCESSFUL);
										break;
									}
									default:
										break;
								 }
								 break;
							}
							case GET_USER_LIST_CMD:
							{
								int N; //alloc space for output users
								int user_no = 0;

								N = get_users_from_file_count();
								/*
								 * Get the users from file
								 */
								user_t **users_from_file;
								user_t **output_users = NULL;
								if (users_from_file == NULL)
									printf("Users from file is null\n");

								output_users = (user_t **) malloc(N * sizeof(user_t *));
								get_users_from_file(users_from_file);
								/*
								 * Get logged in users
								 */
								user_no = get_users_in_order(users_from_file, users, output_users);
								printf("No of users = %d\n", user_no);
								if (output_users == NULL){
									printf("No user has been authenticated\n");
									break;
								} else {
									/*
									 * Check if there is no user logged in 
									 */
									if (user_no == 0) {
										send_client_code(i, GETUSERLIST_EMPTY);
										break;
									}
									/*
									 * Send code for successfull command
									 */
									send_client_code(i, GETUSERLIST_SUCCESSFUL);
									/*
									 * Send the number of users first
									 */
									char message[BUFLEN] = "";
									sprintf(message, "%d", user_no);
									send_client_message(i, message);
									for (int j = 0; j < user_no; ++j){
										printf("Got user %s\n", output_users[j]->username);
										/*
										 * Send the username of each logged in user
										 */
										send_client_message(i, output_users[j]->username);
									}
								}
								break;
							}
							default:
							{
								break;
							}
						} //endswitch code
					}//end else recv
				} 
			}
		}
     }
	
	/*
	 * Closing remarks
	 */
	close(server_sock);
	
	
	return 0;
}
