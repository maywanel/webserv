#pragma once

#include "LocationConfig.hpp"

enum ErrorStatus {
    ERROR_NONE = 0,
    ERROR_BAD_REQUEST = 400,
    ERROR_NOT_FOUND = 404,
    ERROR_METHOD_NOT_ALLOWED = 405,
    ERROR_PAYLOAD_TOO_LARGE = 413,
    ERROR_FORBIDDEN = 403,
    ERROR_INTERNAL_SERVER_ERROR = 500,
};

struct ServerConfig {
    private:
        std::string                 _host;
        std::vector<int>            _port;
        std::string                 _root;
        std::vector<std::string>    _index;
        std::vector<std::string>    _server_names;
        std::map<int, std::string>  _error_pages;
        size_t                      _client_max_body_size;
        bool                        _client_max_body_size_set;
        std::vector<LocationConfig> _locations;
    public:
        ServerConfig();
        const std::string& getHost() const;
        void setHost(const std::string& host);
        const std::vector<int>& getPort() const;
        void setPort(const std::vector<int>& port);
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
        bool isClientMaxBodySizeSet() const;
        void setClientMaxBodySizeSet(bool client_max_body_size_set);
};
