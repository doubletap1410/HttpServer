#ifndef SOCKETFD_H
#define SOCKETFD_H

#include <unistd.h>
#include <utility>
#include <string>

void setDirectory(const char *dir);

int socket_set_nonblock(int sock);

ssize_t socket_fd_write(int sock, void *buf, ssize_t buflen, int fd);
ssize_t socket_fd_read(int sock, void *buf, ssize_t bufsize, int *fd);

void child_process(int sock, int number);
std::pair<pid_t, int> make_child(int number);

std::string parseHttpGet(const char *buf, ssize_t size);
std::string parseUrl(const std::string &url);
std::string getPostfix(const std::string &url);
bool checkFile(const std::string &filename);
std::string readFile(const std::string &filename);
std::string HttpResponse200(const char *buf, ssize_t size);
std::string HttpResponse404();

#endif // SOCKETFD_H
