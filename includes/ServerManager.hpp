#include "WebservConfig.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <stdexcept>
#define MAX_EVENTS 1024

#include "Client.hpp"

struct ListenSocket {
    int fd;
    std::string ip;
    int port;
    std::vector<ServerConfig> virtual_servers; 
};

class ServerManager {
    private:
        std::vector<ListenSocket>   _listening_sockets;
        WebServConfig               _config;
        std::map<int, Client>       _clients;
        ErrorStatus                 _error;

    public:
        ServerManager(const WebServConfig& config);
        ~ServerManager();
        void setupSockets();
        bool isListeningSocket(int fd);
        void run();
};