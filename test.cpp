#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <fstream>

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket!" << std::endl;
        return 1;
    }
    fcntl(server_fd, F_SETFL, O_NONBLOCK);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port!" << std::endl;
        return 1;
    }
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Failed to listen!" << std::endl;
        return 1;
    }
    std::cout << "Server is listening on port 8080..." << std::endl;
    struct pollfd fds[1];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    int ret = poll(fds, 1, -1);
    if (ret < 0) {
        std::cerr << "Poll failed!" << std::endl;
        return 1;
    }
    if (fds[0].revents & POLLIN) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            std::cerr << "Failed to grab connection!" << std::endl;
            return 1;
        }
        std::cout << "A client connected!" << std::endl;
        char buffer[1024] = {0};
        read(client_fd, buffer, 1024);
        std::cout << "The browser said:\n" << buffer << std::endl;
        std::ifstream file("hello.html");
        std::string html_content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        std::string response = "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " + std::to_string(html_content.length()) + "\n\n" + html_content;
        write(client_fd, response.c_str(), response.length());
        close(client_fd);
    }
    close(server_fd);
    return 0;
}