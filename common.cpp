#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h> // exit()

#include <fstream>

#include <cstring>
#include "common.h"

std::string directory;

void setDirectory(const char *dir)
{
    directory = dir;
}

int socket_set_nonblock(int sock)
{
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(sock, F_GETFL, 0)))
        flags = 0;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(sock, FIOBIO, &flags);
#endif
}

ssize_t socket_fd_write(int sock, void *buf, ssize_t buflen, int fd)
{
    ssize_t size;
    struct msghdr msg;
    struct iovec iov;
    union { struct cmsghdr cmsghdr; char control[CMSG_SPACE(sizeof(int))]; } cmsgu;
    struct cmsghdr *cmsg;

    iov.iov_base = buf;
    iov.iov_len = buflen;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (fd != -1) {
        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);

        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;

        *((int *)CMSG_DATA(cmsg)) = fd;
    } else {
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        printf("not passing fd\n");
    }

    size = sendmsg(sock, &msg, 0);
    if (size < 0)
        perror("sendmsg");
    return size;
}

ssize_t socket_fd_read(int sock, void *buf, ssize_t bufsize, int *fd)
{
    ssize_t size;
    if (fd) {
        struct msghdr msg;
        struct iovec iov;
        union { struct cmsghdr cmsghdr; char control[CMSG_SPACE(sizeof(int))]; } cmsgu;
        struct cmsghdr *cmsg;

        iov.iov_base = buf;
        iov.iov_len = bufsize;

        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        msg.msg_control = cmsgu.control;
        msg.msg_controllen = sizeof(cmsgu.control);

        size = recvmsg(sock, &msg, 0);
        if (size < 0) {
            perror("recvmsg");
            *fd = -1;
            return 0;
        }

        cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
            if (cmsg->cmsg_level != SOL_SOCKET) {
                fprintf(stderr, "Invalid cmsg_level %d\n", cmsg->cmsg_level);
                *fd = -1;
                return 0;
            }
            if (cmsg->cmsg_type != SCM_RIGHTS) {
                fprintf(stderr, "Invalid cmsg_type %d\n", cmsg->cmsg_type);
                *fd = -1;
                return 0;
            }
            *fd = *((int *)CMSG_DATA(cmsg));
        }
        else
            *fd = -1;
    }
    else {
        size = read(sock, buf, bufsize);
        if (size < 0) {
            perror("read");
            *fd = -1;
        }
    }
}

void child_process(int sock, int number) {
    char fd_buff[16];
    char buff[1024];
    while (true) {
        // get fd
        int fd;
        socket_fd_read(sock, fd_buff, sizeof(fd_buff), &fd);
        if (fd == -1)
            continue;

        // Work with income socket
        ssize_t size = recv(fd, buff, sizeof(buff), MSG_NOSIGNAL);
        if (size == 0)
            break;

        buff[size] = '\0';

        FILE *log = fopen("/home/box/error.log", "w");

        std::string url = parseHttpGet(buff, size);
        fprintf(log, "URL1: %s\n", url.c_str());
        url = parseUrl(url);
        fprintf(log, "URL2: %s\n", url.c_str());

        fprintf(log, "URL3: %s\n", getPostfix(url).c_str());

        std::string response;
        if (getPostfix(url) != "html") {
            response = HttpResponse404();
        }
        else {
            std::string filename = directory + url;
            fprintf(log, "FILE: %s\n", filename.c_str());
            if (checkFile(filename)) {
                std::string content = readFile(filename);
                response = HttpResponse200(content.c_str(), content.size());
            }
            else {
                response = HttpResponse404();
            }
        }

        fprintf(log, "RESPONSE: %s\n", response.c_str());
        send(fd, response.c_str(), response.length(), MSG_NOSIGNAL);
        close(fd);
    }
    close(sock);
}

std::pair<pid_t, int> make_child(int number) {
    static const uint8_t parent_soket = 0;
    static const uint8_t child_soket = 1;

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        perror("socketpair");
        return std::make_pair(-1, -1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return std::make_pair(-1, -1);
    }
    else if (pid == 0) { // child
        close(sv[parent_soket]);
        child_process(sv[child_soket], number);
        exit(0);
    }
    // parent
    close(sv[child_soket]);
    return std::make_pair(pid, sv[parent_soket]);
}

std::string parseHttpGet(const char *buf, ssize_t size)
{
    std::string type;
    std::string url;
    std::string version;

    int number = 0;
    ssize_t counter = 0;
    std::string currstring;
    while (counter < size) {
        char c = buf[counter++];
        if (c == '\r' && buf[counter++] == '\n') {
            version = currstring;
            break;
        }

        if (c == ' ') {
            if (!currstring.empty()) {
                switch (number++) {
                case 0:
                    type = currstring;
                    break;
                case 1:
                    url = currstring;
                    break;
                default:
                    break;
                }
            }
            currstring.clear();
        }
        else
            currstring.push_back(c);
    }

    if (type == "GET" && version == "HTTP/1.0")
        return url;
    else
        return "";
}

std::string HttpResponse200(const char *buf, ssize_t size)
{
    char res[200 + size];
    const char * format = "HTTP/1.0 200 OK\r\n"
                          "Content-length: %d\r\n"
                          "Connection: close\r\n"
                          "Content-Type: text\\html\r\n"
                          "\r\n"
                          "%s";
    int s = sprintf(res, format, size, buf);
    res[s] = '\0';

    return res;
}

std::string HttpResponse404()
{
    return "HTTP/1.0 404 NOT FOUND\r\n"
           "Content-length: 0\r\n"
           "Connection: close\r\n"
           "Content-Type: text\\html\r\n"
           "\r\n";
}

std::string parseUrl(const std::string &url)
{
    std::size_t c = url.find("?");
    return url.substr(0, c);
}

std::string getPostfix(const std::string &url)
{
    std::size_t c = url.find(".");
    if (c != std::string::npos)
        return url.substr(c + 1);
    return "";
}

bool checkFile(const std::string &filename)
{
    return (access(filename.c_str(), F_OK) != -1);
}

std::string readFile(const std::string &filename)
{
    std::ifstream in(filename.c_str());
    std::string contents((std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>());
    return contents;
}
