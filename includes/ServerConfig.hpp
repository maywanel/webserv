#pragma once

#include "LocationConfig.hpp"

struct ServerConfig {
    private:
        std::string                 _host;
        int                         _port;
        std::string                 _root;
        std::vector<std::string>    _index;
        std::vector<std::string>    _server_names;
        std::map<int, std::string>  _error_pages;
        size_t                      _client_max_body_size; // Max bytes for client requests
        std::vector<LocationConfig> _locations;            // All the routes belonging to this server
    public:
        ServerConfig();
        const std::string& getHost() const;
        void setHost(const std::string& host);
        int getPort() const;
        void setPort(int port);
        const std::string& getRoot() const;
        void setRoot(const std::string& root);
        const std::vector<std::string> & getIndex() const;
        void setIndex(const std::vector<std::string> & index);
        const std::vector<std::string>& getServerNames() const;
        void setServerNames(const std::vector<std::string>& server_names);
        const std::map<int, std::string>& getErrorPages() const;
        void setErrorPages(const std::map<int, std::string>& error_pages);
        size_t getClientMaxBodySize() const;
        void setClientMaxBodySize(size_t client_max_body_size);
        const std::vector<LocationConfig>& getLocations() const;
        void setLocations(const std::vector<LocationConfig>& locations);
};
