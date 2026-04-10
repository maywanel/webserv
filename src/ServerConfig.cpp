#include "ServerConfig.hpp"

ServerConfig::ServerConfig() : _client_max_body_size_set(false) {}

const std::string& ServerConfig::getHost() const {
    return _host;
}
void ServerConfig::setHost(const std::string& host) {
    this->_host = host;
}

const std::vector<int>& ServerConfig::getPort() const {
    return _port;
}
void ServerConfig::setPort(const std::vector<int>& port) {
    this->_port = port;
}

const std::string& ServerConfig::getRoot() const {
    return _root;
}
void ServerConfig::setRoot(const std::string& root) {
    this->_root = root;
}

const std::vector<std::string> & ServerConfig::getIndex() const {
    return _index;
}
void ServerConfig::setIndex(const std::vector<std::string> & index) {
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

bool ServerConfig::isClientMaxBodySizeSet() const {
    return _client_max_body_size_set;
}
void ServerConfig::setClientMaxBodySizeSet(bool client_max_body_size_set) {
    this->_client_max_body_size_set = client_max_body_size_set;
}

