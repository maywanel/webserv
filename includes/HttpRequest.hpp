#pragma once

#include "ServerConfig.hpp"
#include <string>
#include <map>
#include <vector>

class HttpRequest {
    private:
        std::string                         _method;
        std::string                         _uri;
        std::string                         _version;
        std::map<std::string, std::string>  _headers;
        std::string                         _body_file_path;
    public:
        HttpRequest();
        ~HttpRequest();
        std::vector<std::string> tokenizeHeader(const std::string& Header);
        void parseRequest(const std::string& request);
        const std::string& getMethod() const;
        void setMethod(const std::string& method);
        const std::string& getUri() const;
        void setUri(const std::string& uri);
        const std::string& getVersion() const;
        void setVersion(const std::string& version);
        const std::map<std::string, std::string>& getHeaders() const;
        void setHeaders(const std::map<std::string, std::string>& headers);
        const std::string& getBodyFilePath() const;
        void setBodyFilePath(const std::string& body_file_path);
};