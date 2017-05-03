#include <unistd.h>

#ifndef LIBNETFILES_H
#define LIBNETFILES_H

#define INVALID_FILE_MODE 250
#define UNRES_MODE 0
#define EXCL_MODE 1
#define TRANS_MODE 2

// netopen(const char *pathname, int flags);
ssize_t netread(int fildes, void *buf, size_t nbyte);
ssize_t netwrite(int fildes, const void *buf, size_t nbyte);

//Returns a network file descriptor
int netopen(const char *pathname, int flags);
int netclose(int fd);

//Set up metadata for hostname
//hostname can be an ip address or loc, for example basic.cs.rutgers.edu or 119....

int netserverinit(char * hostname, int filemode); // (for extension A)

#endif
