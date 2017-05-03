#include "libnetfiles.h"
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int file_mode = -1;
int port = 19210;
struct hostent *server = NULL;

// Helper functions//
/*
Turns a string into an int (string needs to have numbers in it)
*/
int string_to_int(char *string) {
  char *temp;
  return strtol(string, &temp, 10);
}

/*
Converts an int to a string
*/
char *int_to_string(int integer) {
  int maxNumDigits = 12;
  char *return_string = (char *)malloc(maxNumDigits + 1);
  sprintf(return_string, "%d", integer);
  return return_string;
}

/*
Returns -1 on failure and sets errno appropriately
Otherwise returns the created network socket.
*/
int createsocket() {
  int sockfd = 0;
  struct sockaddr_in serv_addr;
  // char buffer[256];

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    return -1;
  }

  // Erase data in n bytes
  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET; // Type of address
  // Copy net address to sockaddr_in.s_addr (struct)
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
  // Set sockaddr_in port. Convert port integer to network short
  serv_addr.sin_port = htons(port);

  // Connect takes socket descriptor, sockaddr_in, and size of sockaddr_in
  // struct  It returns 0 if sucessful.
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    return -1; // Takes care of ETIMEOUT
  }
  return sockfd;
}

int netserverinit(char *hostname, int filemode) {
  if (filemode < 0 || filemode > 2) {
    errno = EACCES;
    return -1;
  }
  file_mode = filemode;
  server = gethostbyname(hostname);
  if (server == NULL) {
    h_errno = HOST_NOT_FOUND;
    return -1;
  }
  return 0;
}

int netopen(const char *pathname, int flags) {
  if (server == NULL) {
    h_errno = HOST_NOT_FOUND;
    return -1;
  }
  if (flags < 0 || flags > 2) {
    errno = EACCES;
    return -1;
  }

  int sockfd = createsocket();
  if(sockfd == -1){
    return -1;
  }
  // convert flags variable to string
  char *flags_string = int_to_string(flags);

  // Open + pathname + flags_string + 2 spaces
  char *buffer = malloc((5 + strlen(pathname) + 1 + strlen(flags_string) + 3));
  strcat(buffer, "open^");
  strcat(buffer, pathname);
  strcat(buffer, "^");
  strcat(buffer, flags_string);
  strcat(buffer, "^");
  strcat(buffer, int_to_string(file_mode));

  int numbytesWritten = write(sockfd, buffer, strlen(buffer));

  // Write up to strlen(buffer) bytes from buffer into sockfd
  // Returns numbytesWritten
  if (numbytesWritten == -1) {
    return -1;
  }

  // Get server's response
  char response[256];
  int numbytesRead = read(sockfd, response, 255); // read from server
  if (numbytesRead == -1) {
    return -1;
  }

  // printf("[%s], Message to server: %s\n", __func__, buffer);
  // printf("[%s], Response from server: %s\n", __func__, response);

  char *first_word_of_message = strtok(response, "^");
  if (strcmp(first_word_of_message, "error") == 0) {

    // set errno
    char *errnostring = strtok(NULL, " ");
    errno = string_to_int(errnostring);
    return -1;
  }

  // Convert the string FD into a int and return it
  int filedescriptor = string_to_int(first_word_of_message);
  close(sockfd);
  return filedescriptor;
}

ssize_t netread(int fildes, void *buf, size_t nbyte) {
  if (server == NULL) {
    h_errno = HOST_NOT_FOUND;
    return -1;
  }
  int sockfd = createsocket();
  if (sockfd == -1) {
    return -1;
  }
  char *fd_string = int_to_string(fildes);
  char *numbytes_string = int_to_string(nbyte);

  // "read" + fildes + #bytes + 4 (2 spaces + null terminator)
  char *msg_to_server = (char *)malloc(5 + strlen(fd_string) + 1 + strlen(numbytes_string) + 1);

  strcat(msg_to_server, "read^");
  strcat(msg_to_server, fd_string);
  strcat(msg_to_server, "^");
  strcat(msg_to_server, numbytes_string);
  strcat(msg_to_server, "\0");

  int numbytesWritten = write(sockfd, msg_to_server, strlen(msg_to_server));
  if (numbytesWritten < 0) {
    return -1;
  }

  // response from server
  char *response = (char *)malloc(nbyte + 16);

  int numbytesRead = read(sockfd, response, nbyte + 16); // read from server
  if (numbytesRead == -1) {
    return -1;
  }
  // printf("[%s], Message to server: %s\n", __func__, msg_to_server);
  // printf("[%s], Response from server: %s\n", __func__, response);

  char delimiter = '^';
  char *first_word_of_message = (char *)malloc(strlen(response));
  strncpy(first_word_of_message, response, strlen(response));

  int i = 0;
  first_word_of_message++; // move one past the delimiter
  char *servermsg = first_word_of_message;
  while (servermsg[i] != delimiter) {
    servermsg += sizeof(char);
  }
  servermsg[i] = '\0';
  // Move servermsg to after second occurance of delimiter. That is where the
  // server message starts
  servermsg = &servermsg[i + 1];

  if (strcmp(first_word_of_message, "error") == 0) {
    errno = string_to_int(servermsg);
    return -1;
  }

  if(strcmp(response, "")){
    errno = ECONNRESET;
    return -1;
  }
  // Copy message into buffer
  strncpy(buf, servermsg, strlen(servermsg));
  return numbytesRead;
}

ssize_t netwrite(int fildes, const void *buf, size_t nbyte) {
  if (server == NULL) {
    h_errno = HOST_NOT_FOUND;
    return -1;
  }
  int sockfd = createsocket();
  if (sockfd == -1) {
    return -1;
  }
  char *fd_string = int_to_string(fildes);
  char *numbytes_string = int_to_string(nbyte);

  // "write" + fildes + buffer + #bytes + 3 (2 spaces + null terminator)
  char *msg_to_server = (char *)malloc(9 + strlen(fd_string) + strlen(buf) +
                                       strlen(numbytes_string));

  strcat(msg_to_server, "write^");
  strcat(msg_to_server, fd_string);
  strcat(msg_to_server, "^");
  strcat(msg_to_server, buf);
  strcat(msg_to_server, "^");
  strcat(msg_to_server, numbytes_string);
  strcat(msg_to_server, "\0");

  int numbytesWritten = write(sockfd, msg_to_server, strlen(msg_to_server));
  if (numbytesWritten == -1) {
    return -1;
  }

  char *response = (char *)malloc(strlen("error") + 1 + 14);

  int numbytesRead = read(sockfd, response, 20); // read from server
  // printf("[%s], Message to server: %s\n", __func__, msg_to_server);
  // printf("[%s], Response from server: %s\n", __func__, response);
  if(numbytesRead == -1){
    return -1;
  }
  char *first_word_of_message = strtok(response, "^");
  if (strcmp(first_word_of_message, "error") == 0) {

    char *errnostr = strtok(NULL, "^");
    errno = string_to_int(errnostr);
    return -1;
  }
  if(strcmp(response, "") == 0){
    errno = ECONNRESET;
    return -1;
  }
  return string_to_int(first_word_of_message);
}

int netclose(int fd) {
  if (server == NULL) {
    h_errno = HOST_NOT_FOUND;
    return -1;
  }
  int sockfd = createsocket();
  if(sockfd == -1){
    return -1;
  }
  char *fd_string = int_to_string(fd);
  char *msg_to_server = (char *)malloc(7 + strlen(fd_string));

  strcat(msg_to_server, "close^");
  strcat(msg_to_server, fd_string);

  write(sockfd, msg_to_server, strlen(msg_to_server));

  char *response = (char *)malloc(strlen("error") + 15);
  read(sockfd, response, 16); // read from server
  // printf("[%s], Message to server: %s\n", __func__, msg_to_server);
  // printf("[%s], Response from server: %s\n", __func__, response);

  char *first_word_of_message = strtok(response, "^");
  if (strcmp(first_word_of_message, "error") == 0) {

    char *errnostr = strtok(NULL, "^");
    errno = string_to_int(errnostr);
    return -1;
  }
  return string_to_int(first_word_of_message);
}
