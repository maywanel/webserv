#include "ServerManager.hpp"
#include "Client.hpp"
#include <cerrno>

ServerManager::ServerManager(const WebServConfig& config) : _config(config), _error(ERROR_NONE) {}
ServerManager::~ServerManager() {
    for (size_t i = 0; i < _listening_sockets.size(); ++i)
        close(_listening_sockets[i].fd);
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        close(it->first);
}

bool ServerManager::isListeningSocket(int fd) {
    for (size_t i = 0; i < _listening_sockets.size(); ++i)
        if (_listening_sockets[i].fd == fd)
            return true;
    return false;
}

bool g_server_running = true;

void handle_sigint(int signum) {
    (void)signum;
    std::cout << "\nServer shutting down... Cleaning up!" << std::endl;
    g_server_running = false;
}

void ServerManager::setupSockets() {
    std::vector<ServerConfig> servers = _config.getServers();
    std::map<std::pair<std::string, int>, size_t> socket_map;
    for (size_t i = 0; i < servers.size(); ++i) {
        std::string current_ip = servers[i].getHost();
        std::vector<int> current_ports = servers[i].getPort();
        for (size_t p = 0; p < current_ports.size(); ++p) {
            int port = current_ports[p];
            std::pair<std::string, int> ip_port = std::make_pair(current_ip, port);
            if (socket_map.find(ip_port) != socket_map.end()) {
                size_t index = socket_map[ip_port];
                _listening_sockets[index].virtual_servers.push_back(servers[i]);
                continue;
            }
            ListenSocket new_socket;
            new_socket.ip = current_ip;
            new_socket.port = port;
            new_socket.virtual_servers.push_back(servers[i]);
            new_socket.fd = socket(AF_INET, SOCK_STREAM, 0);
            if (new_socket.fd < 0)
                throw std::runtime_error("Failed to create socket");
            int opt = 1;
            if (setsockopt(new_socket.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
                throw std::runtime_error("setsockopt failed");
            if (fcntl(new_socket.fd, F_SETFL, O_NONBLOCK) < 0)
                throw std::runtime_error("fcntl failed to set non-blocking");
            struct sockaddr_in address;
            memset(&address, 0, sizeof(address));
            address.sin_family = AF_INET;
            address.sin_port = htons(port);
            address.sin_addr.s_addr = inet_addr("0.0.0.0");
            if (bind(new_socket.fd, (struct sockaddr*)&address, sizeof(address)) < 0)
                throw std::runtime_error("Bind failed");
            if (listen(new_socket.fd, SOMAXCONN) < 0)
                throw std::runtime_error("Listen failed");
            std::cout << "Server listening on " << current_ip << ":" << port << std::endl;
            _listening_sockets.push_back(new_socket);
            socket_map[ip_port] = _listening_sockets.size() - 1;
        }
    }
}

void ServerManager::run() {
    setupSockets();
    extern bool g_server_running;
    signal(SIGINT, handle_sigint);
    int epoll_fd = epoll_create(EPOLL_CLOEXEC); 
    if (epoll_fd == -1) throw std::runtime_error("epoll_create failed");
    for (size_t i = 0; i < _listening_sockets.size(); ++i) {
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = _listening_sockets[i].fd;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, _listening_sockets[i].fd, &ev) == -1)
            throw std::runtime_error("epoll_ctl failed on listening socket");
    }
    struct epoll_event events[MAX_EVENTS];
    std::cout << "Server is now running and waiting for connections..." << std::endl;
    while (g_server_running) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 2000);
        checkTimeouts(epoll_fd);
        if (num_events == -1) {
            if (!g_server_running) break;
            std::cerr << "epoll_wait error" << std::endl;
            continue;
        }
        for (int i = 0; i < num_events; ++i) {
            int current_fd = events[i].data.fd;
            uint32_t current_event = events[i].events;
            if (_cgi_to_client_fd.find(current_fd) != _cgi_to_client_fd.end()) {
                int client_fd = _cgi_to_client_fd[current_fd];
                std::map<int, Client>::iterator client_it = _clients.find(client_fd);
                if (client_it == _clients.end()) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    close(current_fd);
                    _cgi_to_client_fd.erase(current_fd);
                    continue;
                }
                Client& client = client_it->second;
                char buffer[4096];
                int bytes_read = read(current_fd, buffer, sizeof(buffer));
                if (bytes_read > 0)
                    client.appendCgiOutput(buffer, bytes_read); 
                if (bytes_read == 0 || (current_event & EPOLLHUP) || (current_event & EPOLLERR)) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    close(current_fd);
                    _cgi_to_client_fd.erase(current_fd);
                    client.parseCgiResponse(); 
                    struct epoll_event mod_ev;
                    mod_ev.events = EPOLLOUT;
                    mod_ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &mod_ev);
                }
                continue; 
            }
            if (current_event & (EPOLLERR | EPOLLHUP)) {
                if (isListeningSocket(current_fd)) {
                    std::cerr << "Listener reported EPOLLERR/EPOLLHUP on fd: " << current_fd << std::endl;
                    continue;
                }
                std::cerr << "Error or disconnect on fd: " << current_fd << std::endl;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                close(current_fd);
                _clients.erase(current_fd);
                continue;
            }
            if (isListeningSocket(current_fd)) {
                while (true) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(current_fd, (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        if (errno == EINTR)
                            continue;
                        if (errno == ECONNABORTED)
                            continue;
                        std::cerr << "Accept failed" << std::endl;
                        break;
                    }
                    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
                        close(client_fd);
                        continue;
                    }
                    struct epoll_event client_ev;
                    client_ev.events = EPOLLIN; 
                    client_ev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) == -1) {
                        std::cerr << "Failed to add client to epoll" << std::endl;
                        close(client_fd);
                    } else {
                        std::cout << "New client connected on fd: " << client_fd << std::endl;
                        std::vector<ServerConfig> matched_virtual_servers;
                        for (size_t j = 0; j < _listening_sockets.size(); ++j) {
                            if (_listening_sockets[j].fd == current_fd) {
                                matched_virtual_servers = _listening_sockets[j].virtual_servers;
                                break;
                            }
                        }
                        _clients.insert(std::make_pair(client_fd, Client(client_fd, matched_virtual_servers)));
                    }
                }
                continue;
            }
            else if (current_event & EPOLLIN) {
                char buffer[4096];
                ssize_t bytes_read = recv(current_fd, buffer, sizeof(buffer), 0);
                if (bytes_read <= 0) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                    close(current_fd);
                    _clients.erase(current_fd);
                    continue;
                }
                std::map<int, Client>::iterator it = _clients.find(current_fd);
                if (it == _clients.end())
                    continue;
                Client& client = it->second;
                try {
                    client.appendRequestData(buffer, bytes_read);
                } catch (const std::exception& e) {
                    std::cout << e.what() << std::endl;
                    _error = ERROR_BAD_REQUEST;
                }
                if (client.getState() == STATE_PROCESSING) {
                    if (_error != ERROR_NONE)
                        std::cout << "Request Fully Received for fd: " << current_fd << std::endl;
                    struct epoll_event mod_ev;
                    mod_ev.events = EPOLLOUT;
                    mod_ev.data.fd = current_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, current_fd, &mod_ev);
                }
            }
            else if (current_event & EPOLLOUT) {
                std::map<int, Client>::iterator it = _clients.find(current_fd);
                if (it == _clients.end())
                    continue;
                Client& client = it->second;
                if (client.getState() == STATE_PROCESSING) {
                    std::string raw_path = client.resolveLocation();
                    std::string target_path = client.getFinalPath(raw_path);
                    client.generateResponse(target_path, _error);
                }
                if (client.getState() == STATE_CGI_READING) {
                    int cgi_fd = client.getCgiReadFd();
                    struct epoll_event cgi_ev;
                    cgi_ev.events = EPOLLIN;
                    cgi_ev.data.fd = cgi_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cgi_fd, &cgi_ev);
                    _cgi_to_client_fd[cgi_fd] = current_fd;
                    struct epoll_event mod_ev;
                    mod_ev.events = EPOLLIN;
                    mod_ev.data.fd = current_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, current_fd, &mod_ev);
                }
                if (client.getState() == STATE_SENDING_RESPONSE) {
                    std::string response = client.getResponseBuffer();
                    int bytes_sent = send(current_fd, response.c_str(), response.length(), 0);
                    if (bytes_sent > 0) {
                        response.erase(0, bytes_sent);
                        if (response.empty()) {
                            std::cout << "Successfully sent full response to client " << current_fd << std::endl;
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                            close(current_fd);
                            _clients.erase(current_fd);
                        }
                    } else if (bytes_sent <= 0) {
                        std::cerr << "Send failed or client disconnected" << std::endl;
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current_fd, NULL);
                        close(current_fd);
                        _clients.erase(current_fd);
                    }
                    _error = ERROR_NONE;
                }
            }
        }
    }
}

void ServerManager::checkTimeouts(int epoll_fd) {
    time_t current_time = std::time(NULL);
    std::map<int, Client>::iterator it = _clients.begin();
    while (it != _clients.end()) {
        Client& client = it->second;
        int client_fd = it->first;
        if (client.isTimedOut(current_time, 10)) {
            if (client.getState() == STATE_CGI_READING) {
                std::cout << "CGI Timeout on FD: " << client_fd << ". Assassinating PID " << client.getCgiPid() << std::endl;
                kill(client.getCgiPid(), SIGKILL);
                waitpid(client.getCgiPid(), NULL, 0);
                int cgi_fd = client.getCgiReadFd();
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cgi_fd, NULL);
                close(cgi_fd);
                _cgi_to_client_fd.erase(cgi_fd);
                client.generateErrorResponse(ERROR_GATEWAY_TIMEOUT);
                struct epoll_event mod_ev;
                mod_ev.events = EPOLLOUT;
                mod_ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &mod_ev);
                ++it;
            } 
            else {
                std::cout << "Client Timeout on FD: " << client_fd << " - Dropping connection." << std::endl;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                close(client_fd);
                std::map<int, Client>::iterator to_erase = it;
                ++it;
                _clients.erase(to_erase);
            }
        } 
        else
            ++it;
    }
}
