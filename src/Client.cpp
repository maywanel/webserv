#include "Client.hpp"
#include "ServerManager.hpp"

Client::Client()
    : _fd(-1), _virtual_servers(), _matched_server(NULL), _state(STATE_READING_HEADERS), _last_activity(std::time(NULL)), _bytes_received(0), _content_length(0) {
}

Client::Client(int fd, const std::vector<ServerConfig>& virtual_servers)
    : _fd(fd), _virtual_servers(virtual_servers), _matched_server(NULL), _state(STATE_READING_HEADERS), _last_activity(std::time(NULL)), _bytes_received(0), _content_length(0) {
}

Client::Client(const Client& other)
    : _fd(other._fd), _virtual_servers(other._virtual_servers),
    _matched_server(other._matched_server), _request_buffer(other._request_buffer),
    _response_buffer(other._response_buffer), _state(other._state),
    _last_activity(other._last_activity), _temp_filename(other._temp_filename),
    _bytes_received(other._bytes_received), _content_length(other._content_length) {
}

Client& Client::operator=(const Client& other) {
    if (this != &other) {
        _fd = other._fd;
        _virtual_servers = other._virtual_servers;
        _matched_server = other._matched_server;
        _request_buffer = other._request_buffer;
        _response_buffer = other._response_buffer;
        _state = other._state;
        _last_activity = other._last_activity;
        _temp_filename = other._temp_filename;
        _bytes_received = other._bytes_received;
        _content_length = other._content_length;
    }
    return *this;
}

Client::~Client() {
    if (_body_file.is_open()) {
        _body_file.close();
    }
}

void Client::appendRequestData(const char* data, ssize_t len) {
    if (_state == STATE_READING_HEADERS) {
        _request_buffer.append(data, len);
        if (isHeaderComplete()) {
            _request.parseRequest(_request_buffer);
            if (!_virtual_servers.empty()) {
                _matched_server = &_virtual_servers[0];
                std::map<std::string, std::string> headers = _request.getHeaders();
                std::map<std::string, std::string>::iterator it = headers.find("host");
                if (it != headers.end()) {
                    std::string host = it->second;
                    size_t colon_pos = host.find(':');
                    std::string host_name = host;
                    if (colon_pos != std::string::npos)
                        host_name = host.substr(0, colon_pos);
                    bool found = false;
                    for (size_t i = 0; i < _virtual_servers.size(); ++i) {
                        std::vector<std::string> names = _virtual_servers[i].getServerNames();
                        for (size_t j = 0; j < names.size(); ++j) {
                            if (names[j] == host_name || names[j] == host) {
                                _matched_server = &_virtual_servers[i];
                                found = true;
                                break;
                            }
                        }
                        if (found) break;
                    }
                }
            }
            _state = STATE_READING_BODY;
            _content_length = parseContentLength(_request_buffer);
            _request.setBodyInFile(_content_length > 1048576);
            if (_request.isBodyInFile()) {
                std::stringstream ss;
                ss << "upload_temp_" << _fd << ".tmp";
                _temp_filename = ss.str();
                _body_file.open(_temp_filename.c_str(), std::ios::binary | std::ios::app);
            }
            std::string leftover_body = extractBodyFromBuffer(_request_buffer);
            if (!leftover_body.empty()) {
                if (_request.isBodyInFile())
                    _body_file.write(leftover_body.c_str(), leftover_body.length());
                else
                    _request.setBodyString(leftover_body);
                _bytes_received += leftover_body.length();
            }
            if (_bytes_received >= _content_length) {
                if (_request.isBodyInFile() && _body_file.is_open())
                    _body_file.close();
                _state = STATE_PROCESSING;
            }
        }
    } 
    else if (_state == STATE_READING_BODY) {
        if (_request.isBodyInFile()) {
            _body_file.write(data, len);
            _bytes_received += len;
        }
        else{
            _request.setBodyString(_request.getBodyString() + std::string(data, len));
            _bytes_received += len;
        }
        if (_bytes_received >= _content_length) {
            if (_request.isBodyInFile())
                _body_file.close();
            _state = STATE_PROCESSING;
        }
    }
}

bool Client::isHeaderComplete() const {
    return _request_buffer.find("\r\n\r\n") != std::string::npos;
}

void Client::updateActivityTime() {
    _last_activity = std::time(NULL);
}

int Client::getFd() const { return _fd; }

ClientState Client::getState() const { return _state; }

void Client::setState(ClientState state) { _state = state; }

const std::string& Client::getRequestBuffer() const { return _request_buffer; }
const std::string& Client::getResponseBuffer() const { return _response_buffer; }

void Client::appendResponseData(const char* data, ssize_t len) {
    _response_buffer.append(data, len);
    updateActivityTime();
}

const ServerConfig* Client::getMatchedServer() const { return _matched_server; }
void Client::setMatchedServer(const ServerConfig* server) { _matched_server = server; }
time_t Client::getLastActivityTime() const { return _last_activity; }

bool Client::isTimedOut(time_t current_time, time_t timeout_seconds) const {
    return (current_time - _last_activity) > timeout_seconds;
}

std::string Client::extractBodyFromBuffer(const std::string& buffer) {
    size_t header_end_pos = buffer.find("\r\n\r\n");
    if (header_end_pos != std::string::npos)
        return buffer.substr(header_end_pos + 4);
    return "";
}

size_t Client::parseContentLength(const std::string& buffer) {
    size_t pos = buffer.find("Content-Length:");
    if (pos != std::string::npos) {
        pos += 15;
        while (pos < buffer.length() && std::isspace(buffer[pos]))
            pos++;
        size_t end_pos = buffer.find("\r\n", pos);
        if (end_pos != std::string::npos) {
            std::string length_str = buffer.substr(pos, end_pos - pos);
            return std::atoi(length_str.c_str());
        }
    }
    return 0;
}

std::string Client::resolveLocation() {
    if (!_matched_server)
        return "";
    std::string uri = _request.getUri();
    const std::vector<LocationConfig>& locations = _matched_server->getLocations();
    
    int best_index = -1;
    size_t longest_match = 0;
    for (size_t i = 0; i < locations.size(); ++i) {
        std::string loc_path = locations[i].getPath();
        if (uri.compare(0, loc_path.length(), loc_path) == 0) {
            if (loc_path.length() > longest_match) {
                longest_match = loc_path.length();
                best_index = i;
            }
        }
    }
    std::string final_root;
    if (best_index == -1)
        final_root = _matched_server->getRoot(); 
    else
        final_root = locations[best_index].getRoot();
    _matched_location = (best_index == -1) ? NULL : &locations[best_index];
    if (!final_root.empty() && final_root[final_root.length() - 1] == '/' && !uri.empty() && uri[0] == '/')
        final_root.erase(final_root.length() - 1);
    return final_root + uri;
}

int Client::checkPath(const std::string& path) {
    struct stat file_info;
    if (stat(path.c_str(), &file_info) == -1)   return PATH_NOT_FOUND; 
    if (S_ISDIR(file_info.st_mode))             return PATH_IS_DIR;
    if (S_ISREG(file_info.st_mode))             return PATH_IS_FILE;
    return PATH_UNKNOWN;
}

std::string Client::getFinalPath(std::string raw_path) {
    int type = checkPath(raw_path);
    if (type == PATH_IS_FILE)
        return raw_path; 
    if (type == PATH_IS_DIR) {
        std::vector<std::string> index_file;
        if (_matched_location)
            index_file = _matched_location->getIndex();
        std::string base_path = raw_path;
        if (base_path[base_path.length() - 1] != '/')
            base_path += "/";
        for (size_t i = 0; i < index_file.size(); ++i) {
            std::string test_path = base_path + index_file[i];
            if (checkPath(test_path) == PATH_IS_FILE)
                return test_path;
        }
        return raw_path;
    }
    return ""; 
}

void Client::generateResponse(const std::string& target_path, ErrorStatus error) {
    std::string response;
    if (error != ERROR_NONE) {
        generateErrorResponse(error);
        return;
    }
    if (target_path == "") {
        generateErrorResponse(ERROR_NOT_FOUND);
        return;
    }
    else if (checkPath(target_path) == PATH_IS_DIR) {
        generateErrorResponse(ERROR_FORBIDDEN);
        return;
    }
    else {
        std::ifstream file(target_path.c_str(), std::ios::binary);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            
            std::ostringstream oss;
            oss << content.length();
            std::string mime_type = getMimeType(target_path);
            response = "HTTP/1.1 200 OK\r\n"
                       "Content-Type: " + mime_type + "\r\n"
                       "Content-Length: " + oss.str() + "\r\n\r\n" + content;
            file.close();
        }
        else {
            generateErrorResponse(ERROR_NOT_FOUND);
            return;
        }
    }
    _response_buffer = response;
    _state = STATE_SENDING_RESPONSE;
}

void Client::generateErrorResponse(int error) {
    std::string response;
    std::map<int, std::string>::iterator it;
    std::map<int, std::string> error_pages;
    std::string status_msg;

    if (_matched_server) {
        error_pages = _matched_server->getErrorPages();
        it = error_pages.find(error);
    }
    bool has_custom_page = (_matched_server && it != error_pages.end());
    if (error == ERROR_NOT_FOUND) {
        status_msg = "404 Not Found";
        if (!has_custom_page)
            response = "HTTP/1.1 404 Not Found\r\nContent-Length: 13\r\n\r\n404 Not Found";
    } else if (error == ERROR_METHOD_NOT_ALLOWED) {
        status_msg = "405 Method Not Allowed";
        if (!has_custom_page)
            response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 22\r\n\r\n405 Method Not Allowed";
    } else if (error == ERROR_PAYLOAD_TOO_LARGE) {
        status_msg = "413 Payload Too Large";
        if (!has_custom_page)
            response = "HTTP/1.1 413 Payload Too Large\r\nContent-Length: 22\r\n\r\n413 Payload Too Large";
    } else if (error == ERROR_FORBIDDEN) {
        status_msg = "403 Forbidden";
        if (!has_custom_page)
            response = "HTTP/1.1 403 Forbidden\r\nContent-Length: 13\r\n\r\n403 Forbidden";
    } else {
        status_msg = "500 Internal Server Error";
        if (!has_custom_page)
            response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 25\r\n\r\n500 Internal Server Error";
    }
    if (has_custom_page) {
        std::string err_path = _matched_server->getRoot();
        if (!err_path.empty() && err_path[err_path.length() - 1] != '/')
            err_path += "/";
        if (!it->second.empty() && it->second[0] == '/')
            err_path += it->second.substr(1);
        else
            err_path += it->second;
        err_path = getFinalPath(err_path);
        if (checkPath(err_path) == PATH_IS_FILE) {
            std::ifstream file(err_path.c_str(), std::ios::binary);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string content = buffer.str();
                
                std::ostringstream oss;
                oss << content.length();
                response = "HTTP/1.1 " + status_msg + "\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: " + oss.str() + "\r\n\r\n" + content;
                file.close();
                _response_buffer = response;
                _state = STATE_SENDING_RESPONSE;
                return;
            }
        }
        if (response.empty()) {
            std::ostringstream default_oss;
            default_oss << "HTTP/1.1 " << status_msg << "\r\nContent-Length: " << status_msg.length() << "\r\n\r\n" << status_msg;
            response = default_oss.str();
        }
    }
    
    _response_buffer = response;
    _state = STATE_SENDING_RESPONSE;
}

std::string Client::getMimeType(const std::string& target_path) {
    size_t dot_pos = target_path.find_last_of('.');
    if (dot_pos == std::string::npos) 
        return "application/octet-stream"; 
    std::string ext = target_path.substr(dot_pos);
    for (size_t i = 0; i < ext.length(); ++i)
        ext[i] = std::tolower(ext[i]);
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css")                   return "text/css";
    if (ext == ".js")                    return "application/javascript";
    if (ext == ".txt")                   return "text/plain";
    if (ext == ".png")                   return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")                   return "image/gif";
    if (ext == ".ico")                   return "image/x-icon";
    if (ext == ".json")                  return "application/json";
    if (ext == ".pdf")                   return "application/pdf";
    if (ext == ".xml")                   return "application/xml";
    return "application/octet-stream"; 
}
