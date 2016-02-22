#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <map>
#include <utility>

#include "common.h"

//#define ENABLE_SIGNALS

static const int max_child = 10;
int children[max_child];
std::map<pid_t, int> process_to_socket;

void sigHandler(int) {
    int status;
    pid_t id = wait(&status);

    int pos = process_to_socket[id];
    process_to_socket.erase(id);

    std::pair<pid_t, int> res = make_child(pos + 1);
    children[pos] = res.second;
    process_to_socket.insert(std::make_pair(res.first, pos));
}

int main(int argc, char *argv[])
{
    // parse parameters
    std::string host;
    std::string port;

    int rez = 0;
    while ( (rez = getopt(argc, argv, "h:p:d:")) != -1) {
        switch (rez) {
        case 'h':
            host = optarg;
            break;
        case 'p':
            port = optarg;
            break;
        case 'd':
            setDirectory(optarg);
            break;
        default:
            break;
        }
    }

#ifdef ENABLE_SIGNALS
    // Set action on SIGCHILD
    {
        __sigset_t set;
        sigfillset(&set);

        struct sigaction action;
        action.sa_handler = &sigHandler;
        action.sa_mask = set;
        action.sa_flags = SA_RESTART;

        sigaction(SIGCHLD, &action, NULL);
    }
#endif

    pid_t pid = fork();
    if (pid == 0) {
        // Create Master Socket
        int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        int optval = 1;
        setsockopt(MasterSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        struct sockaddr_in SockAddr;
        SockAddr.sin_family = AF_INET;
        SockAddr.sin_port = htons(atoi(port.c_str()));
        inet_pton(AF_INET, host.c_str(), &(SockAddr.sin_addr));
        bind(MasterSocket, (struct sockaddr *)(&SockAddr), sizeof(SockAddr));

        listen(MasterSocket, SOMAXCONN);

        // Create child process
        for (int i = 0; i < max_child; ++i) {
            std::pair<pid_t, int> res = make_child(i + 1);
            children[i] = res.second;
            process_to_socket.insert(std::make_pair(res.first, i));
        }

        int counter = 0;
        // Getting incoming connections
        char buff[] = "";
        while (true) {
            int incoming_socket = accept(MasterSocket, 0, 0);
            int size = -1;
            while (size < 0) {
                size = socket_fd_write(children[counter], buff, sizeof(buff), incoming_socket);
                counter = (counter + 1) % max_child;
            }
        }
    }

    return 0;
}

