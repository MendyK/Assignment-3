#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define UNRES_MODE 0
#define EXCL_MODE 1
#define TRANS_MODE 2

struct args {
  int cli_sockfd;
  char *message;
};

int string_to_int(char *string) {
  char *temp;
  return strtol(string, &temp, 10);
}

char *int_to_string(int integer) {
  int maxNumDigits = 12;
  char *return_string = (char *)malloc(maxNumDigits + 1);
  sprintf(return_string, "%d", integer);
  return return_string;
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////BEGIN TABLE/////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
// Example:
//  struct descriptors *a1 = calloc(1, (sizeof(struct descriptors)));
//  (*a1).clientFD = 40;
//  (*a1).serverFD = -40;
//  (*a1).filepath = "jj";
//  add_table_element(a1);
//  remove_table_element(-40);
//  search_table(-40);

struct descriptors {
  int normalFD;
  int ourFD;
  char *filepath;
  int filemode;
  int flags;
};

// Array of struct descriptors pointers
struct descriptors *table[500];
#define ARRAY_SIZE sizeof(table) / sizeof(table[0])
pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

struct descriptors *search_table(int ourFD) {
  int i = 0;
  for (; i < ARRAY_SIZE; i++) {
    if (table[i] != NULL) {
      if ((*table[i]).ourFD == ourFD) {
        return table[i];
      }
    }
  }
  return NULL;
}

/**
Returns the first element with the filepath described
*/
struct descriptors *search_table_by_filepath(char *fp) {
  int i = 0;
  for (; i < ARRAY_SIZE; i++) {
    if (table[i] != NULL) {
      if (strcmp((*table[i]).filepath, fp) == 0) {
        return table[i];
      }
    }
  }
  return NULL;
}

/**
Adds an element to the table.
Returns 0 on success, -1 if it fails
*/
int add_table_element(struct descriptors *element) {
  pthread_mutex_lock(&table_mutex);
  // avoid adding duplicates
  if (search_table((*element).ourFD) != NULL) {
    pthread_mutex_unlock(&table_mutex);
    return -1;
  }

  // Loop through array until we find an empty spot
  int i = 0;
  for (; i < ARRAY_SIZE; i++) {
    if (table[i] == NULL) {

      // Create struct on heap since all threads share the same heap.
      table[i] = calloc(1, sizeof(struct descriptors));
      (*table[i]).normalFD = (*element).normalFD;
      (*table[i]).ourFD = (*element).ourFD;
      (*table[i]).filepath = (*element).filepath;
      (*table[i]).filemode = (*element).filemode;
      (*table[i]).flags = (*element).flags;

      pthread_mutex_unlock(&table_mutex);
      return 0;
    }
  }
  pthread_mutex_unlock(&table_mutex);
  return -1;
}

/**
Removes an element from the table.
Returns 0 on success, -1 if it fails
*/
int remove_table_element(int networkFD) {
  pthread_mutex_lock(&table_mutex);

  int i = 0;
  for (; i < ARRAY_SIZE; i++) {
    if ((*table[i]).ourFD == networkFD) {
      table[i] = NULL;
      pthread_mutex_unlock(&table_mutex);
      return 0;
    }
  }
  pthread_mutex_unlock(&table_mutex);
  return -1;
}

void print_table() {
  int i = 0;
  for (; i < ARRAY_SIZE; i++) {
    if (table[i] != NULL) {

      struct descriptors *elem = table[i];

      printf("[%i] Element ourFD: %i, normalFD: %i, filepath: %s, filemode: %d, flags: %d, \n", i,
             (*elem).ourFD, (*elem).normalFD, (*elem).filepath, (*elem).filemode, (*elem).flags);
    }
  }
}

/**
Returns a boolean that lets you know if any other client has it open in write
mode.
*/
bool is_open_in_writemode(char *filepath) {
  int i = 0;
  for (; i < ARRAY_SIZE; i++) {
    if (table[i] != NULL) {
      struct descriptors current = *(table[i]);

      // printf("current.filepath: %s\n", current.filepath);
      // printf("current.flags: %i\n", current.flags);

      if (strcmp(current.filepath, filepath) == 0) {
        if (current.flags == O_RDWR || current.flags == O_WRONLY) {
          return true;
        }
      }
    }
  }
  return false;
}

/**
Checks the file descriptor table to see if there is a file with filepath of the
parameter open in write, with exclusive or transaction mode on.
*/
bool is_file_open_exlORtrans_write(char *filepath)
{
  int i = 0;
  for (; i < ARRAY_SIZE; i++) {
    if (table[i] != NULL) {
      struct descriptors current = *(table[i]);

      if (strcmp(current.filepath, filepath) == 0) {
        if (current.flags == O_RDWR || current.flags == O_WRONLY) {
          if(current.filemode == TRANS_MODE || current.filemode == EXCL_MODE){
            return true;
          }
        }
      }
    }
  }
  return false;
}
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////END TABLE///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

void error(char *msg) {
  perror(msg);
  exit(1);
}

char *handle_open(char *message) {
  char *message_cpy = (char *)malloc(strlen(message) + 1);
  sprintf(message_cpy, "%s", message);
  char *filepath = malloc(sizeof(message_cpy));
  int flags = 0;
  char *token = strtok(message_cpy, "^");
  int fmode = -1;

  // Tokenize strings
  int i = 0;
  while (token != NULL) {
    token = strtok(NULL, "^");
    if (i == 0) {
      strncpy(filepath, token, strlen(token));
    } else if (i == 1) {
      char *tempstring = malloc(10);
      strncpy(tempstring, token, strlen(token));
      flags = string_to_int(tempstring);
    } else if (i == 2) {
      char *tempstring = malloc(10);
      strncpy(tempstring, token, strlen(token));
      fmode = string_to_int(tempstring);
    }
    i++;
  }
  if (fmode == EXCL_MODE) {
    if (flags == O_RDWR || flags == O_WRONLY) {
      // check if any other clients have it open in write mode.
      bool isopen = is_open_in_writemode(filepath);
      if (isopen) {
        char *returnmessage = malloc(17 * sizeof(char)); // 4 + max int size + "\0"
        sprintf(returnmessage, "error^%d", EACCES);
        return returnmessage;
      }
    }
  } else if (fmode == TRANS_MODE) {
    struct descriptors *search = search_table_by_filepath(filepath);
    if (search != NULL) { // means some other client has this file open
      char *returnmessage = malloc(17 * sizeof(char));
      sprintf(returnmessage, "error^%d", EACCES);
      return returnmessage;
    }
  }else if (fmode == UNRES_MODE){
    //if any file is open (in write) in any mode other than unrestricted, return an error.
    bool is_somefile_open_in_write = is_file_open_exlORtrans_write(filepath);
    if(is_somefile_open_in_write){
      char *returnmessage = malloc(17 * sizeof(char));
      sprintf(returnmessage, "error^%d", EACCES);
      return returnmessage;
    }
  }
  // Find out if the current client is allowed to access the file.
  // If not return a message with error...EACCES

  int regular_fd = open(filepath, flags);
  int customFD = -regular_fd;

  if (regular_fd == -1) {
    char *returnmessage = malloc(17 * sizeof(char)); // 4 + max int size + "\0"
    sprintf(returnmessage, "error^%d", errno);
    return returnmessage;

  } else if (regular_fd == 1) {
    // Create a random file descriptor # but make sure it isn't in our table
    // first
    do {
      customFD = -((rand() % 500) + 2);
    } while (search_table(customFD) != NULL);
  }

  // Add to table
  struct descriptors *newentry = calloc(1, (sizeof(struct descriptors)));
  (*newentry).normalFD = regular_fd;
  (*newentry).ourFD = customFD;
  (*newentry).filepath = filepath;
  (*newentry).filemode = fmode;
  (*newentry).flags = flags;

  add_table_element(newentry);
  return int_to_string(customFD);
}

char *handle_read(char *message) {
  char *message_cpy = (char *)malloc(strlen(message) + 1);
  sprintf(message_cpy, "%s", message);
  char *token = strtok(message_cpy, "^");
  int fd = 0;
  int numbytes_to_read = 0;

  int i = 0;
  while (token != NULL) {
    token = strtok(NULL, "^");
    if (i == 0) {
      char *tempfd_str = malloc(13);
      strncpy(tempfd_str, token, strlen(token));
      fd = string_to_int(tempfd_str);
    } else if (i == 1) {
      char *tempstring = malloc(13);
      strncpy(tempstring, token, strlen(token));
      numbytes_to_read = string_to_int(tempstring);
    }
    i++;
  }

  struct descriptors *fd_info = search_table(fd);
  if (fd_info == NULL) {

    char *returnmessage = malloc(20 * sizeof(char)); // 4 + max int size + "\0"
    sprintf(returnmessage, "error^%d", EBADF); // EBADF (bad file descriptor)
    return returnmessage;
  }
  char *buf = (char *)malloc(numbytes_to_read + 1);
  int numbytes_read = read((*fd_info).normalFD, buf, numbytes_to_read);

  if (numbytes_read == -1) {
    char *returnmessage = malloc(20 * sizeof(char)); // 4 + max int size + "\0"
    sprintf(returnmessage, "error^%d", errno); // EBADF (bad file descriptor)
    return returnmessage;
  }
	else if (numbytes_read == 0) {
    return "0^\0";
  }
  char *returnstring = malloc(numbytes_read + 16);
  strcat(returnstring, "^");
  strcat(returnstring, int_to_string(numbytes_read));
  strcat(returnstring, "^");
  strcat(returnstring, buf);
  return returnstring;
}
char *handle_write(char *message) {
  char delimiter = '^';
  char *fd_string = (char *)malloc(strlen(message));
  char *string_to_write = (char *)malloc(strlen(message));
  char *numbytes_to_write = (char *)malloc(strlen(message));
  int fdstrlen = 0, str_to_wr_len = 0, nby_to_wr_len = 0;

  strncpy(fd_string, message, strlen(message));
  strncpy(string_to_write, message, strlen(message));
  strncpy(numbytes_to_write, message, strlen(message));

  int delimiter_count = 0;
  int i = 0;

  while (message[i] != '\0') {
    if (message[i] == delimiter) {
      delimiter_count++;

      if (delimiter_count == 1) {
        fdstrlen = i;
        fd_string = &fd_string[i + 1];

      } else if (delimiter_count == 2) {
        fdstrlen = i - fdstrlen;
        fd_string[fdstrlen - 1] = '\0';

        string_to_write = &string_to_write[i + 1];
        str_to_wr_len = i;
      } else if (delimiter_count == 3) {
        str_to_wr_len = i - str_to_wr_len;
        string_to_write[str_to_wr_len - 1] = '\0';

        nby_to_wr_len = i;
        numbytes_to_write = &numbytes_to_write[i + 1];
      }
    }
    i++;
  }

  nby_to_wr_len = i - str_to_wr_len;
  numbytes_to_write[nby_to_wr_len - 1] = '\0';

  int serverfd = string_to_int(fd_string);
  struct descriptors *fdstruct = search_table(serverfd);
  if (fdstruct == NULL) {
    // File not open / invalid fd
    char *returnmessage = malloc(20 * sizeof(char)); // 4 + max int size + "\0"
    sprintf(returnmessage, "error^%d", EBADF); // EBADF (bad file descriptor)
    return returnmessage;
  }
  int numbyteswritten = write((*fdstruct).normalFD, string_to_write,
                              string_to_int(numbytes_to_write));
  if (numbyteswritten == -1) {
    char *returnmessage = malloc(20 * sizeof(char)); // 4 + max int size + "\0"
    sprintf(returnmessage, "error^%d", errno); // EBADF (bad file descriptor)
    return returnmessage;
  }

  return int_to_string(numbyteswritten);
}

char *handle_close(char *message) {
  strtok(message, "^");
  char *strfd = strtok(NULL, "^");
  int ourfd = string_to_int(strfd);

  struct descriptors *desc_struct = search_table(ourfd);

  if (desc_struct == NULL) {
    char *returnmessage = malloc(20 * sizeof(char)); // 4 + max int size + "\0"
    sprintf(returnmessage, "error^%d", EBADF); // EBADF (bad file descriptor)
    return returnmessage;
  }
  int success = close((*desc_struct).normalFD);
  if (success == -1) {
    char *returnmessage = malloc(20 * sizeof(char)); // 4 + max int size + "\0"
    sprintf(returnmessage, "error^%d", errno); // EBADF (bad file descriptor)
    return returnmessage;
  }

  remove_table_element((*desc_struct).ourFD);
  return int_to_string(0);
}

// First run without threading (i.e. assuming there is only one client) for
// simplicity.
void *clientservice(void *structure) {
  struct args *clientinfo = structure;
  int client_socketFD = (*clientinfo).cli_sockfd;
  char *client_message = (*clientinfo).message;

  char *client_message_cpy = (char *)malloc(strlen(client_message) + 1);
  sprintf(client_message_cpy, "%s", client_message);

  char *first_word_of_message = strtok(client_message_cpy, "^");
  char *response = NULL;
  if (strcmp(first_word_of_message, "open") == 0) {
    response = handle_open(client_message);
  } else if (strcmp(first_word_of_message, "read") == 0) {
    response = handle_read(client_message);
  } else if (strcmp(first_word_of_message, "write") == 0) {
    response = handle_write(client_message);
  } else if (strcmp(first_word_of_message, "close") == 0) {
    response = handle_close(client_message);
  } else {
  }

  // send file descriptor back to client
  write(client_socketFD, response, strlen(response) + 1);
  close(client_socketFD);

  // exit thread
  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char *argv[]) {
  int sockfd = 0, newsockfd = 0, portno = 19210, clientstructlen = 0;
  char buffer[256];
  struct sockaddr_in serv_addr, cli_addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    error("ERROR opening socket");
  }

  // Set socket option to reuse port
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0) {
    error("setsockopt(SO_REUSEADDR) failed");
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY; // address of client //Accept
                                          // connections from anyone (i.e. any
                                          // client)
  serv_addr.sin_port = htons(portno); // convert the int to a network short

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    error("ERROR on binding");
  }

  // max # connections == 5
  // listen returns 0 if sucessful
  if (listen(sockfd, 5) != 0) {
    error("ERROR on listen");
  }
  clientstructlen = sizeof(cli_addr);

  while (1) {
    // accept returns a new socket that connects to the other side (the client)
    newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,
                       (socklen_t *)&clientstructlen);
    if (newsockfd < 0) {
      error("ERROR on accept");
    }

    bzero(buffer, 256);
    int numbytesread = read(newsockfd, buffer, 255);
    if (numbytesread < 0) {
      error("ERROR reading from socket");
    }
    struct args *message = calloc(1, sizeof(struct args));
    (*message).cli_sockfd = newsockfd;
    (*message).message = buffer;

    pthread_t clientservice_thread;
    pthread_create(&clientservice_thread, NULL, clientservice, message);
  }
  return 0;
}
