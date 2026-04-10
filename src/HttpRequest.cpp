#include "HttpRequest.hpp"

HttpRequest::HttpRequest() {}
HttpRequest::~HttpRequest() {}

const std::string& HttpRequest::getMethod() const {
    return _method;
}
void HttpRequest::setMethod(const std::string& method) {
    _method = method;
}

const std::string& HttpRequest::getUri() const {
    return _uri;
}
void HttpRequest::setUri(const std::string& uri) {
    _uri = uri;
}

const std::string& HttpRequest::getVersion() const {
    return _version;
}
void HttpRequest::setVersion(const std::string& version) {
    _version = version;
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
    return _headers;
}
void HttpRequest::setHeaders(const std::map<std::string, std::string>& headers) {
    _headers = headers;
}

const std::string& HttpRequest::getBodyFilePath() const {
    return _body_file_path;
}
void HttpRequest::setBodyFilePath(const std::string& body_file_path) {
    _body_file_path = body_file_path;
}

std::vector<std::string> HttpRequest::tokenizeHeader(const std::string& header) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t pos = 0;
    size_t end = header.find("\r\n\r\n");
    while (start < end) {
        pos = header.find("\r\n", start);
        if (pos == std::string::npos || pos > end)
            pos = end;
        std::string line = header.substr(start, pos - start);
        tokens.push_back(line);
        start = pos + 2;
    }
    return tokens;
}

void HttpRequest::parseRequest(const std::string& request) {
    std::vector<std::string> tokens = tokenizeHeader(request);
    if (tokens.empty())
        return;
    std::istringstream request_line(tokens[0]);
    request_line >> _method >> _uri >> _version;

    for (size_t i = 1; i < tokens.size(); ++i) {
        size_t colon_pos = tokens[i].find(':');
        if (colon_pos != std::string::npos) {
            std::string key = tokens[i].substr(0, colon_pos);
            std::string value = tokens[i].substr(colon_pos + 1);
            if (!key.empty() && (key[key.length() - 1] == ' ' || key[key.length() - 1] == '\t'))
                throw std::runtime_error("Syntax error: Header key cannot end with whitespace");
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            for (size_t j = 0; j < key.length(); ++j)
                key[j] = std::tolower(key[j])   ;
            _headers[key] = value;
        }
    }
    std::cout << "Parsed Request: " << _method << " " << _uri << " " << _version << std::endl;
    for (std::map<std::string, std::string>::iterator it = _headers.begin(); it != _headers.end(); ++it)
        std::cout << "Header: " << it->first << " => " << it->second << std::endl;
}