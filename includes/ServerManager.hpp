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
#include "Client.hpp"
#define MAX_EVENTS 1024

enum ErrorStatus {
    ERROR_NONE = 0,
    ERROR_BAD_REQUEST = 400,
    ERROR_NOT_FOUND = 404,
    ERROR_METHOD_NOT_ALLOWED = 405,
    ERROR_PAYLOAD_TOO_LARGE = 413,
    ERROR_FORBIDDEN = 403,
    ERROR_INTERNAL_SERVER_ERROR = 500,
};

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