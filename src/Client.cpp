#include "Client.hpp"
#include "ServerManager.hpp"
#include <iostream>
#include <sstream>

static const ServerConfig* rebindMatchedServerPtr(const std::vector<ServerConfig>& src_servers,
                                                  const std::vector<ServerConfig>& dst_servers,
                                                  const ServerConfig* src_ptr) {
    if (!src_ptr)
        return NULL;
    for (size_t i = 0; i < src_servers.size(); ++i) {
        if (&src_servers[i] == src_ptr) {
            if (i < dst_servers.size())
                return &dst_servers[i];
            break;
        }
    }
    return NULL;
}

static const LocationConfig* rebindMatchedLocationPtr(const ServerConfig* src_server,
                                                      const ServerConfig* dst_server,
                                                      const LocationConfig* src_loc_ptr) {
    if (!src_server || !dst_server || !src_loc_ptr)
        return NULL;
    const std::vector<LocationConfig>& src_locs = src_server->getLocations();
    const std::vector<LocationConfig>& dst_locs = dst_server->getLocations();
    for (size_t i = 0; i < src_locs.size(); ++i) {
        if (&src_locs[i] == src_loc_ptr) {
            if (i < dst_locs.size())
                return &dst_locs[i];
            break;
        }
    }
    return NULL;
}

Client::Client()
    : _fd(-1), _virtual_servers(), _matched_server(NULL), _request_buffer(), _response_buffer(),
      _state(STATE_READING_HEADERS), _last_activity(std::time(NULL)), _temp_filename(), _body_file(),
      _bytes_received(0), _content_length(0), _matched_location(NULL), _request(), _session_manager(),
      _cgi_pid(-1), _cgi_write_fd(-1), _cgi_read_fd(-1), _cgi_buffer(), _is_chunked(false) {}

Client::Client(int fd, const std::vector<ServerConfig>& virtual_servers)
    : _fd(fd), _virtual_servers(virtual_servers), _matched_server(NULL), _request_buffer(), _response_buffer(),
      _state(STATE_READING_HEADERS), _last_activity(std::time(NULL)), _temp_filename(), _body_file(),
      _bytes_received(0), _content_length(0), _matched_location(NULL), _request(), _session_manager(),
      _cgi_pid(-1), _cgi_write_fd(-1), _cgi_read_fd(-1), _cgi_buffer(), _is_chunked(false) {}

Client::Client(const Client& other)
    : _fd(other._fd), _virtual_servers(other._virtual_servers),
    _matched_server(NULL), _request_buffer(other._request_buffer),
    _response_buffer(other._response_buffer), _state(other._state),
    _last_activity(other._last_activity), _temp_filename(other._temp_filename),
    _bytes_received(other._bytes_received), _content_length(other._content_length),
    _matched_location(NULL), _request(other._request), _session_manager(other._session_manager),
    _cgi_pid(other._cgi_pid), _cgi_write_fd(other._cgi_write_fd), _cgi_read_fd(other._cgi_read_fd),
    _cgi_buffer(other._cgi_buffer), _is_chunked(other._is_chunked) {
    _matched_server = rebindMatchedServerPtr(other._virtual_servers, _virtual_servers, other._matched_server);
    _matched_location = rebindMatchedLocationPtr(other._matched_server, _matched_server, other._matched_location);
}

Client& Client::operator=(const Client& other) {
    if (this != &other) {
        _fd = other._fd;
        _virtual_servers = other._virtual_servers;
        _matched_server = rebindMatchedServerPtr(other._virtual_servers, _virtual_servers, other._matched_server);
        _request_buffer = other._request_buffer;
        _response_buffer = other._response_buffer;
        _state = other._state;
        _last_activity = other._last_activity;
        _temp_filename = other._temp_filename;
        _bytes_received = other._bytes_received;
        _content_length = other._content_length;
        _matched_location = rebindMatchedLocationPtr(other._matched_server, _matched_server, other._matched_location);
        _request = other._request;
        _session_manager = other._session_manager;
        _cgi_pid = other._cgi_pid;
        _cgi_write_fd = other._cgi_write_fd;
        _cgi_read_fd = other._cgi_read_fd;
        _cgi_buffer = other._cgi_buffer;
        _is_chunked = other._is_chunked;
    }
    return *this;
}

Client::~Client() {
    if (_body_file.is_open())
        _body_file.close();
}

pid_t Client::getCgiPid() const { return _cgi_pid; }
int Client::getCgiWriteFd() const { return _cgi_write_fd; }
int Client::getCgiReadFd() const { return _cgi_read_fd; }

std::string Client::extractSessionId(const std::string& cookie_header) {
    size_t pos = cookie_header.find("session=");
    if (pos == std::string::npos)
        return "";
    pos += 8;
    size_t end = cookie_header.find(';', pos);
    if (end == std::string::npos) 
        return cookie_header.substr(pos);
    return cookie_header.substr(pos, end - pos);
}

void Client::appendRequestData(const char* data, ssize_t len) {
    if (_state == STATE_READING_HEADERS) {
        _request_buffer.append(data, len);
        if (isHeaderComplete()) {
            _state = STATE_PROCESSING;
            _request.parseRequest(_request_buffer);
            _state = STATE_READING_HEADERS;
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
            std::map<std::string, std::string> headers = _request.getHeaders();
            if (headers.count("transfer-encoding") && headers["transfer-encoding"] == "chunked")
                _is_chunked = true;
            else {
                _is_chunked = false;
                _content_length = parseContentLength(_request_buffer);
            }
            std::stringstream ss;
            ss << "upload_temp_" << _fd << ".tmp";
            _temp_filename = ss.str();
            _request.setBodyFilePath(_temp_filename);
            _body_file.open(_temp_filename.c_str(), std::ios::binary | std::ios::app);
            std::string leftover_body = extractBodyFromBuffer(_request_buffer);
            if (_is_chunked)
                _cgi_buffer += leftover_body;
            else if (!leftover_body.empty()) {
                _body_file.write(leftover_body.c_str(), leftover_body.length());
                _bytes_received += leftover_body.length();
            }
            if (!_is_chunked && _bytes_received >= _content_length){
                _body_file.close();
                _state = STATE_PROCESSING;
            }
            else if (_is_chunked && _cgi_buffer.find("0\r\n\r\n") != std::string::npos) {
                std::string pureData = decodeChunked(_cgi_buffer);
                _body_file.write(pureData.c_str(), pureData.length());
                _body_file.close();
                _cgi_buffer.clear();
                _state = STATE_PROCESSING;
            }
        }
    } 
    else if (_state == STATE_READING_BODY) {
        if (_is_chunked) {
            _cgi_buffer.append(data, len);
            if (_cgi_buffer.find("0\r\n\r\n") != std::string::npos) {
                std::string pureData = decodeChunked(_cgi_buffer);
                _body_file.write(pureData.c_str(), pureData.length());
                _body_file.close();
                _cgi_buffer.clear();
                _state = STATE_PROCESSING;
            }
        }
        else {
            _body_file.write(data, len);
            _bytes_received += len;
            if (_bytes_received >= _content_length) {
                _body_file.close();
                _state = STATE_PROCESSING;
            }
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
    if (_matched_location) {
        std::vector<std::string> allowed_methods = _matched_location->getMethods();
        if (!allowed_methods.empty()) {
            bool method_allowed = false;
            for (size_t i = 0; i < allowed_methods.size(); ++i) {
                if (_request.getMethod() == allowed_methods[i]) {
                    method_allowed = true;
                    break;
                }
            }
            if (!method_allowed) {
                generateErrorResponse(ERROR_METHOD_NOT_ALLOWED);
                return;
            }
        }
    }
    if (_request.getMethod() == "DELETE") {
        if (checkPath(target_path) == PATH_NOT_FOUND) {
            generateErrorResponse(ERROR_NOT_FOUND);
            return;
        }
        if (checkPath(target_path) == PATH_IS_DIR) {
            generateErrorResponse(ERROR_FORBIDDEN); 
            return;
        }
        if (std::remove(target_path.c_str()) == 0) {
            _response_buffer = "HTTP/1.1 204 No Content\r\n\r\n";
            _state = STATE_SENDING_RESPONSE;
            std::cout << "Successfully deleted file: " << target_path << std::endl;
        }
        else
            generateErrorResponse(ERROR_FORBIDDEN);
        return;
    }
    if (_request.getMethod() == "POST") {
        std::string upload_dir = _matched_location ? _matched_location->getUploadDir() : "";
        if (!upload_dir.empty()) {
            std::string final_upload_dir = _matched_server->getRoot();
            if (final_upload_dir[final_upload_dir.length() - 1] != '/') final_upload_dir += "/";
            if (upload_dir[0] == '/') upload_dir = upload_dir.substr(1);
            final_upload_dir += upload_dir;
            if (final_upload_dir[final_upload_dir.length() - 1] != '/') final_upload_dir += "/";
            std::map<std::string, std::string> headers = _request.getHeaders();
            std::string content_type = headers.count("content-type") ? headers["content-type"] : "";
            size_t boundary_pos = content_type.find("boundary=");
            bool success = false;
            std::string saved_path = "";
            if (boundary_pos != std::string::npos) {
                std::string boundary = content_type.substr(boundary_pos + 9);
                saved_path = Multipartextraction(_temp_filename, final_upload_dir, boundary);
                if (!saved_path.empty()) {
                    std::remove(_temp_filename.c_str());
                    success = true;
                }
            } else {
                size_t last_slash = _request.getUri().find_last_of('/');
                std::string uri_name = _request.getUri().substr(last_slash + 1);
                if (uri_name.empty() || uri_name == "upload") uri_name = "curl_upload_" + _temp_filename;
                saved_path = final_upload_dir + uri_name;
                success = (std::rename(_temp_filename.c_str(), saved_path.c_str()) == 0);
            }
            if (success) {
                _response_buffer = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
                _state = STATE_SENDING_RESPONSE;
                std::cout << "Successfully saved file as: " << saved_path << std::endl;
            }
            else
                generateErrorResponse(ERROR_INTERNAL_SERVER_ERROR);
            return;
        }
    }
    if (_request.getUri() == "/session") {
        std::map<std::string, std::string> headers = _request.getHeaders();
        std::string session_id = "";
        if (headers.count("cookie"))
            session_id = extractSessionId(headers["cookie"]);
        bool is_new_user = false;
        if (session_id.empty() || !_session_manager.isValidSession(session_id)) {
            session_id = _session_manager.createNewSession();
            is_new_user = true;
        }
        _session_manager.incrementVisits(session_id);
        int visits = _session_manager.getVisits(session_id);
        std::ostringstream html;
        html << "<!DOCTYPE html><html><head><title>Session Test</title></head><body style='text-align:center; font-family:sans-serif; margin-top:50px;'>"
             << "<h1>Webserv Session Tracker</h1>"
             << "<p>Your unique Session ID: <b>" << session_id << "</b></p>"
             << "<h2>You have visited this page <span style='color:red;'>" << visits << "</span> times.</h2>"
             << "<p>Refresh the page to see the counter go up!</p>"
             << "</body></html>";
        std::string body = html.str();
        std::ostringstream content_length;
        content_length << body.length();
        std::string response_headers = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
        if (is_new_user)
            response_headers += "Set-Cookie: session=" + session_id + "; Path=/; Max-Age=3600\r\n";
        response_headers += "Content-Length: " + content_length.str() + "\r\n\r\n";
        _response_buffer = response_headers + body;
        _state = STATE_SENDING_RESPONSE;
        return;
    }
    if (target_path == "") {
        generateErrorResponse(ERROR_NOT_FOUND);
        return;
    }
    if (target_path == "") {
        generateErrorResponse(ERROR_NOT_FOUND);
        return;
    }
    else if (checkPath(target_path) == PATH_IS_DIR) {
        if (_matched_location && _matched_location->getAutoindex()) {
            std::string autoindex_html = generateAutoindex(target_path, _request.getUri());
            if (autoindex_html.empty()) {
                generateErrorResponse(ERROR_INTERNAL_SERVER_ERROR);
                return;
            }
            std::ostringstream oss;
            oss << autoindex_html.length();
            response =  "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: " + oss.str() + "\r\n\r\n" + autoindex_html;
            _response_buffer = response;
            _state = STATE_SENDING_RESPONSE;
            return;
        }
        else {
            generateErrorResponse(ERROR_FORBIDDEN);
            return;
        }
    }
    else {
        bool is_cgi = false;
        if (_matched_location) {
            std::vector<std::string> exts = _matched_location->getCgiExt();
            size_t dot = target_path.find_last_of('.');
            if (dot != std::string::npos) {
                std::string ext = target_path.substr(dot);
                for (size_t i = 0; i < exts.size(); ++i) {
                    if (exts[i] == ext) {
                        is_cgi = true;
                        break;
                    }
                }
            }
        }
        std::cout << "Final target path: " << target_path << " (CGI: " << (is_cgi ? "Yes" : "No") << ")" << std::endl;
        if (is_cgi) {
            bool success = executeCgi(target_path);
            if (!success)
                generateErrorResponse(ERROR_INTERNAL_SERVER_ERROR);
            return;
        }
        std::ifstream file(target_path.c_str(), std::ios::binary);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            
            std::ostringstream oss;
            oss << content.length();
            std::string mime_type = getMimeType(target_path);
            response =  "HTTP/1.1 200 OK\r\n"
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
    } else if (error == ERROR_REQUEST_TIMEOUT) {
        status_msg = "408 Request Timeout";
        if (!has_custom_page)
            response = "HTTP/1.1 408 Request Timeout\r\nContent-Length: 19\r\n\r\n408 Request Timeout";
    } else if (error == ERROR_GATEWAY_TIMEOUT) {
        status_msg = "504 Gateway Timeout";
        if (!has_custom_page)
            response = "HTTP/1.1 504 Gateway Timeout\r\nContent-Length: 19\r\n\r\n504 Gateway Timeout";
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

#include <dirent.h>

std::string Client::generateAutoindex(const std::string& target_path, const std::string& uri) {
    std::string html =  "<!DOCTYPE html>\n<html>\n<head>\n<title>Index of " + uri + "</title>\n"
                        "<style>body{font-family: monospace; font-size: 16px;} "
                        "table{width: 100%; text-align: left;} "
                        "th, td{padding: 5px; border-bottom: 1px solid #ccc;} "
                        "a{text-decoration: none; color: #1a0dab;} "
                        "a:hover{text-decoration: underline;}</style>\n"
                        "</head>\n<body>\n<h1>Index of " + uri + "</h1>\n<hr>\n<table>\n"
                        "<tr><th>Name</th></tr>\n";
    DIR* dir = opendir(target_path.c_str());
    if (dir == NULL)
        return "";
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string file_name = entry->d_name;
        if (file_name == ".") continue;
        std::string link = uri;
        if (link.empty() || link[link.length() - 1] != '/')
            link += "/";
        link += file_name;
        html += "<tr><td><a href=\"" + link + "\">" + file_name + "</a></td></tr>\n";
    }
    closedir(dir);
    html += "</table>\n<hr>\n</body>\n</html>";
    return html;
}

std::string Client::Multipartextraction(const std::string& temp_file, const std::string& upload_dir, const std::string& boundary) {
    std::ifstream in(temp_file.c_str(), std::ios::binary);
    if (!in.is_open()) return "";
    char buffer[8192];
    in.read(buffer, sizeof(buffer));
    std::string header_chunk(buffer, in.gcount());
    std::string real_filename = "uploaded_file.bin";
    size_t filename_pos = header_chunk.find("filename=\"");
    if (filename_pos != std::string::npos) {
        filename_pos += 10;
        size_t filename_end = header_chunk.find("\"", filename_pos);
        if (filename_end != std::string::npos)
            real_filename = header_chunk.substr(filename_pos, filename_end - filename_pos);
    }
    size_t start_pos = header_chunk.find("\r\n\r\n");
    if (start_pos == std::string::npos) return "";
    start_pos += 4;
    in.seekg(0, std::ios::end);
    size_t file_size = in.tellg();
    size_t tail_size = std::min((size_t)8192, file_size);
    in.seekg(-tail_size, std::ios::end);
    in.read(buffer, tail_size);
    std::string tail_chunk(buffer, in.gcount());
    std::string end_boundary = "\r\n--" + boundary + "--";
    size_t end_pos_in_tail = tail_chunk.rfind(end_boundary);
    size_t absolute_end_pos = file_size;
    if (end_pos_in_tail != std::string::npos)
        absolute_end_pos = (file_size - tail_size) + end_pos_in_tail;
    else {
        end_boundary = "--" + boundary + "--";
        end_pos_in_tail = tail_chunk.rfind(end_boundary);
        if (end_pos_in_tail != std::string::npos)
            absolute_end_pos = (file_size - tail_size) + end_pos_in_tail;
    }
    std::string final_path = upload_dir + real_filename;
    in.seekg(start_pos, std::ios::beg);
    std::ofstream out(final_path.c_str(), std::ios::binary);
    size_t bytes_to_copy = absolute_end_pos - start_pos;
    size_t bytes_copied = 0;
    while (bytes_copied < bytes_to_copy) {
        size_t chunk = std::min((size_t)8192, bytes_to_copy - bytes_copied);
        in.read(buffer, chunk);
        out.write(buffer, in.gcount());
        bytes_copied += in.gcount();
    }
    in.close();
    out.close();
    return final_path;
}

std::vector<std::string> Client::buildCgiEnv(const std::string& script_path) {
    std::vector<std::string> env;
    const std::map<std::string, std::string>& headers = _request.getHeaders();
    env.push_back("REQUEST_METHOD=" + _request.getMethod());
    env.push_back("SCRIPT_NAME=" + script_path);
    std::map<std::string, std::string>::const_iterator ct_it = headers.find("content-type");
    env.push_back("CONTENT_TYPE=" + (ct_it != headers.end() ? ct_it->second : ""));
    std::map<std::string, std::string>::const_iterator cl_it = headers.find("content-length");
    env.push_back("CONTENT_LENGTH=" + (cl_it != headers.end() ? cl_it->second : ""));
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    if (_matched_server) {
        const std::vector<std::string>& names = _matched_server->getServerNames();
        env.push_back("SERVER_NAME=" + (names.empty() ? "localhost" : names[0]));
        std::string server_port = "80";
        std::map<std::string, std::string>::const_iterator host_it = headers.find("host");
        if (host_it != headers.end()) {
            size_t colon_pos = host_it->second.find(':');
            if (colon_pos != std::string::npos && colon_pos + 1 < host_it->second.length())
                server_port = host_it->second.substr(colon_pos + 1);
        }
        env.push_back("SERVER_PORT=" + server_port);
    }
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        std::string header_name = "HTTP_" + it->first;
        for (size_t i = 0; i < header_name.length(); ++i) {
            if (header_name[i] == '-')
                header_name[i] = '_';
            else
                header_name[i] = std::toupper(header_name[i]);
        }
        env.push_back(header_name + "=" + it->second);
    }
    return env;
}

bool Client::executeCgi(const std::string& script_path) {
    std::string interpreter = "";
    if (_matched_location) {
        std::vector<std::string> exts = _matched_location->getCgiExt();
        std::vector<std::string> paths = _matched_location->getCgiPath();
        size_t dot_pos = script_path.find_last_of('.');
        if (dot_pos != std::string::npos) {
            std::string ext = script_path.substr(dot_pos);
            for (size_t i = 0; i < exts.size(); ++i) {
                if (exts[i] == ext && i < paths.size()) {
                    interpreter = paths[i];
                    break;
                }
            }
        }
    }
    if (interpreter.empty()) return false;

    int pipe_in[2];
    int pipe_out[2];
    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) return false;
    fcntl(pipe_in[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_in[1], F_SETFL, O_NONBLOCK);
    fcntl(pipe_out[0], F_SETFL, O_NONBLOCK);
    fcntl(pipe_out[1], F_SETFL, O_NONBLOCK);

    pid_t pid = fork();
    if (pid == -1) return false;

    if (pid == 0) {
        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);
        std::vector<std::string> env_vec = buildCgiEnv(script_path);
        char** envp = new char*[env_vec.size() + 1];
        for (size_t i = 0; i < env_vec.size(); ++i) {
            envp[i] = strdup(env_vec[i].c_str());
        }
        envp[env_vec.size()] = NULL;
        char* argv[] = { (char*)interpreter.c_str(), (char*)script_path.c_str(), NULL };
        execve(interpreter.c_str(), argv, envp);
        exit(1); 
    } 
    else {
        close(pipe_in[0]);
        close(pipe_out[1]);
        _cgi_pid = pid;
        _cgi_write_fd = pipe_in[1];
        _cgi_read_fd = pipe_out[0];
        std::string FileName = _request.getBodyFilePath();
        std::ifstream body_file(FileName.c_str(), std::ios::binary);
        if (body_file.is_open()) {
            std::stringstream buffer;
            buffer << body_file.rdbuf();
            std::string body = buffer.str();
            if (!body.empty()) {
                write(_cgi_write_fd, body.c_str(), body.length());
            }
            body_file.close();
        }
        close(_cgi_write_fd); 
        _state = STATE_CGI_READING;
        return true;
    }
}

void Client::appendCgiOutput(const char* data, ssize_t len) {
    _cgi_buffer.append(data, len);
}

void Client::parseCgiResponse() {
    if (_cgi_buffer.empty()) {
        generateErrorResponse(ERROR_INTERNAL_SERVER_ERROR);
        return;
    }
    size_t header_end = _cgi_buffer.find("\r\n\r\n");
    std::string cgi_headers = "";
    std::string cgi_body = "";
    if (header_end != std::string::npos) {
        cgi_headers = _cgi_buffer.substr(0, header_end + 2);
        cgi_body = _cgi_buffer.substr(header_end + 4);
    } else {
        cgi_body = _cgi_buffer;
        cgi_headers = "Content-Type: text/plain\r\n"; 
    }
    std::ostringstream oss;
    oss << cgi_body.length();
    _response_buffer = "HTTP/1.1 200 OK\r\n" 
                        + cgi_headers 
                        + "Content-Length: " + oss.str() + "\r\n\r\n" 
                        + cgi_body;
    _cgi_buffer.clear();
    _state = STATE_SENDING_RESPONSE;
}

std::string Client::decodeChunked(const std::string& chunked_body) {
    std::string decoded = "";
    size_t pos = 0;
    
    while (pos < chunked_body.length()) {
        size_t end_of_hex = chunked_body.find("\r\n", pos);
        if (end_of_hex == std::string::npos) break;
        std::string hex_str = chunked_body.substr(pos, end_of_hex - pos);
        size_t chunk_size = 0;
        std::stringstream ss;
        ss << std::hex << hex_str;
        ss >> chunk_size;
        if (chunk_size == 0) break;
        pos = end_of_hex + 2; 
        decoded += chunked_body.substr(pos, chunk_size);
        pos += chunk_size + 2; 
    }    
    return decoded;
}
