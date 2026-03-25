#include "ServerConfig.hpp"

ServerConfig::ServerConfig() : _host("0.0.0.0"), _port(80), _client_max_body_size(1048576) {}

const std::string& ServerConfig::getHost() const {
    return _host;
}
void ServerConfig::setHost(const std::string& host) {
    this->_host = host;
}

int ServerConfig::getPort() const {
    return _port;
}
void ServerConfig::setPort(int port) {
    this->_port = port;
}

const std::string& ServerConfig::getRoot() const {
    return _root;
}
void ServerConfig::setRoot(const std::string& root) {
    this->_root = root;
}

const std::string& ServerConfig::getIndex() const {
    return _index;
}
void ServerConfig::setIndex(const std::string& index) {
    this->_index = index;
}

const std::vector<std::string>& ServerConfig::getServerNames() const {
    return _server_names;
}
void ServerConfig::setServerNames(const std::vector<std::string>& server_names) {
    this->_server_names = server_names;
}

const std::map<int, std::string>& ServerConfig::getErrorPages() const {
    return _error_pages;
}
void ServerConfig::setErrorPages(const std::map<int, std::string>& error_pages) {
    this->_error_pages = error_pages;
}

size_t ServerConfig::getClientMaxBodySize() const {
    return _client_max_body_size;
}
void ServerConfig::setClientMaxBodySize(size_t client_max_body_size) {
    this->_client_max_body_size = client_max_body_size;
}

const std::vector<LocationConfig>& ServerConfig::getLocations() const {
    return _locations;
}
void ServerConfig::setLocations(const std::vector<LocationConfig>& locations) {
    this->_locations = locations;
}
