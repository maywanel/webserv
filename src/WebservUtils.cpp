#include "WebservConfig.hpp"

WebServConfig::WebServConfig () {}
WebServConfig::~WebServConfig () {}

bool isDirectory(const std::string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
        return false;
    return (info.st_mode & S_IFDIR) != 0;
}

bool isFile(const std::string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
        return false;
    return (info.st_mode & S_IFREG) != 0;
}

bool isNumeric(const std::string& str) {
    if (str.empty()) return false;
    for (size_t i = 0; i < str.length(); ++i)
        if (!std::isdigit(str[i])) return false;
    return true;
}

bool isValidPort(const std::string& portStr) {
    if (!isNumeric(portStr)) return false;
    int port = std::atoi(portStr.c_str());
    return (port >= 1024 && port <= 65535);
}

bool isValidIP(const std::string& ip) {
    if (ip == "localhost") return true;
    int dots = 0;
    std::stringstream ss(ip);
    std::string octet;

    while (std::getline(ss, octet, '.')) {
        if (octet.empty() || octet.length() > 3 || !isNumeric(octet))
            return false;
        int val = std::atoi(octet.c_str());
        if (val < 0 || val > 255)
            return false;
        dots++;
    }
    int actual_dots = 0;
    for (size_t i = 0; i < ip.length(); ++i)
        if (ip[i] == '.')
            actual_dots++;
    return (dots == 4 && actual_dots == 3);
}

long long check_client_max_body_size(const std::string& value) {
    if (value.empty()) return -1;
    char last_char = value[value.length() - 1];
    std::string number_part = value.substr(0, value.length() - 1);
    if (!isNumeric(number_part)) return -1;
    long long size = std::atoll(number_part.c_str());
    if (size < 0) return -1;
    if (last_char == 'K' || last_char == 'k')
        size *= 1024LL;
    else if (last_char == 'M' || last_char == 'm')
        size *= 1024LL * 1024LL;
    else if (last_char == 'G' || last_char == 'g')
        size *= 1024LL * 1024LL * 1024LL;
    else
        return -1;
    return size;
}

template<typename T>
void checkErrorPage(const std::string& code, const T& sl) {
    int errorCode = std::atoi(code.c_str());
    for (size_t i = 0; i < sl.getErrorPages().size(); ++i) {
        if (sl.getErrorPages().find(errorCode) != sl.getErrorPages().end())
            throw std::runtime_error("Config Error: error page code already defined -> " + code);
    }
}

void checkLocationPath(const std::string& path, const std::vector<LocationConfig>& existing_locations) {
    if (path.empty() || path[0] != '/')
        throw std::runtime_error("Config Error: Location path must start with '/' -> " + path);
    for (size_t i = 0; i < existing_locations.size(); ++i)
        if (existing_locations[i].getPath() == path)
            throw std::runtime_error("Config Error: Duplicate location path -> " + path);
}

void WebServConfig::parse(const std::string& filename) {
    std::vector<std::string> tokens = tokenizeConfig(filename);
    ParserState state = STATE_GLOBAL;
    ServerConfig current_server;
    LocationConfig current_location;

    for (size_t i = 0; i < tokens.size(); ++i) {
        std::string token = tokens[i];
        if (token == "server" && state == STATE_GLOBAL) {
            if (i + 1 < tokens.size() && tokens[i + 1] == "{") {
                state = STATE_SERVER;
                current_server = ServerConfig();
                i++;
                continue;
            }
            else
                throw std::runtime_error("Syntax error: Expected '{' after 'server'");
        }
        else if (token == "location" && state == STATE_SERVER) {
            if (i + 2 < tokens.size() && tokens[i + 2] == "{") {
                state = STATE_LOCATION;
                current_location = LocationConfig();
                std::string subpath = (tokens[i + 1].length() > 1 && tokens[i + 1][tokens[i + 1].length() - 1] == '/') ? tokens[i + 1].substr(0, tokens[i + 1].length() - 1) : tokens[i + 1];
                checkLocationPath(subpath, current_server.getLocations());
                current_location.setPath(subpath);
                i += 2;
                continue;
            }
            else
                throw std::runtime_error("Syntax error: Expected '{' after 'location'");
        }
        else if (token == "}") {
            if (state == STATE_LOCATION) {
                std::vector<LocationConfig> locations = current_server.getLocations();
                locations.push_back(current_location);
                current_server.setLocations(locations);
                current_location = LocationConfig();
                state = STATE_SERVER;
            } 
            else if (state == STATE_SERVER) {
                _servers.push_back(current_server);
                state = STATE_GLOBAL;
            } 
            else {
                throw std::runtime_error("Syntax error: Unexpected '}'");
            }
        }
        else if (state == STATE_SERVER) {
            if (token == "listen") {
                if (i + 2 < tokens.size() && tokens[i + 2] == ";") {
                    std::string listen_val = tokens[i + 1];
                    size_t colon_pos = listen_val.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string ip = listen_val.substr(0, colon_pos);
                        std::string port_str = listen_val.substr(colon_pos + 1);
                        if (!isValidIP(ip) || !isValidPort(port_str))
                            throw std::runtime_error("Config Error: Invalid IP:Port in listen directive -> " + listen_val);
                        current_server.setHost(ip);
                        std::vector<int> port_vec;
                        port_vec.push_back(std::atoi(port_str.c_str()));
                        current_server.setPort(port_vec);
                    }
                    else {
                        if (listen_val.find('.') != std::string::npos || listen_val == "localhost") {
                            if (!isValidIP(listen_val))
                                throw std::runtime_error("Config Error: Invalid IP in listen directive -> " + listen_val);
                            current_server.setHost(listen_val);
                        } 
                        else {
                            if (!isValidPort(listen_val))
                                throw std::runtime_error("Config Error: Invalid Port in listen directive -> " + listen_val);
                            std::vector<int> port_vec;
                            port_vec.push_back(std::atoi(listen_val.c_str()));
                            current_server.setPort(port_vec);
                        }
                    }
                    i += 2;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid listen directive");
            }
            else if (token == "server_name") {
                i++;
                std::vector<std::string> server_names = current_server.getServerNames();
                while (i < tokens.size() && tokens[i] != ";")
                    server_names.push_back(tokens[i++]);
                current_server.setServerNames(server_names);
            }
            else if (token == "host") {
                if (current_server.getHost().empty() && i + 2 < tokens.size() && tokens[i + 2] == ";") {
                    if (!isValidIP(tokens[i + 1]))
                        throw std::runtime_error("Config Error: Invalid IP in host directive -> " + tokens[i + 1]);
                    current_server.setHost(tokens[i + 1]);
                    i += 2;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid host directive: " + tokens[i + 1]);
            }
            else if (token == "root") {
                if (current_server.getRoot().empty() && i + 2 < tokens.size() && tokens[i + 2] == ";") {
                    current_server.setRoot(tokens[i + 1]);
                    i += 2;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid root directive: " + tokens[i + 1]);
            }
            else if (token == "index") {
                i++;
                std::vector<std::string> index = current_server.getIndex();
                while (i < tokens.size() && tokens[i] != ";")
                    index.push_back(tokens[i++]);
                current_server.setIndex(index);
            }
            else if (token == "error_page") {
                if (i + 3 < tokens.size() && tokens[i + 3] == ";") {
                    checkErrorPage(tokens[i + 1], current_server);
                    std::map<int, std::string> error_pages = current_server.getErrorPages();
                    error_pages[std::atoi(tokens[i + 1].c_str())] = tokens[i + 2];
                    current_server.setErrorPages(error_pages);
                    i += 3;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid error_page directive");
            }
            else if (token == "client_max_body_size") {
                if (!current_server.isClientMaxBodySizeSet() && i + 2 < tokens.size() && tokens[i + 2] == ";") {
                    long long size = check_client_max_body_size(tokens[i + 1]);
                    if (size == -1)
                        throw std::runtime_error("Config Error: Invalid client_max_body_size value -> " + tokens[i + 1]);
                    current_server.setClientMaxBodySize(size);
                    current_server.setClientMaxBodySizeSet(true);
                    i += 2;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid client_max_body_size directive");
            }
            else
                throw std::runtime_error("Unknown directive in server block: " + token);
        }

        else if (state == STATE_LOCATION) {
            if (token == "root") {
                if (current_location.getRoot().empty() && i + 2 < tokens.size() && tokens[i + 2] == ";") {
                    current_location.setRoot(tokens[i + 1]);
                    i += 2;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid root directive: " + tokens[i + 1]);
            }
            else if (token == "autoindex") {
                if (current_location.isAutoindexSet() == false && i + 2 < tokens.size() && tokens[i + 2] == ";") {
                    current_location.setAutoindex(tokens[i + 1] == "on");
                    current_location.setAutoindexSet(true);
                    i += 2;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid autoindex directive: " + tokens[i + 1]);
            }
            else if (token == "index") {
                i++;
                while (i < tokens.size() && tokens[i] != ";") {
                    std::vector<std::string> indexes = current_location.getIndex();
                    indexes.push_back(tokens[i]);
                    current_location.setIndex(indexes);
                    i++;
                }
            }
            else if (token == "cgi_ext") {
                i++;
                while (i < tokens.size() && tokens[i] != ";") {
                    std::vector<std::string> cgi_ext = current_location.getCgiExt();
                    cgi_ext.push_back(tokens[i]);
                    current_location.setCgiExt(cgi_ext);
                    i++;
                }
            }
            else if (token == "cgi_path") {
                i++;
                while (i < tokens.size() && tokens[i] != ";") {
                    std::vector<std::string> cgi_path = current_location.getCgiPath();
                    cgi_path.push_back(tokens[i]);
                    current_location.setCgiPath(cgi_path);
                    i++;
                }
            }
            else if (token == "upload_dir") {
                if (current_location.getUploadDir().empty() && i + 2 < tokens.size() && tokens[i + 2] == ";") {
                    current_location.setUploadDir(tokens[i + 1]);
                    i += 2;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid upload_dir directive: " + tokens[i + 1]);
            }
            else if (token == "allow_methods" || token == "methods") {
                i++;
                while (i < tokens.size() && tokens[i] != ";") {
                    std::vector<std::string> methods = current_location.getMethods();
                    if (tokens[i] != "GET" && tokens[i] != "POST" && tokens[i] != "DELETE")
                        throw std::runtime_error("Syntax error: Invalid HTTP method in allow_methods directive: " + tokens[i]);
                    methods.push_back(tokens[i]);
                    current_location.setMethods(methods);
                    i++;
                }
            }
            else if (token == "return") {
                bool isSet = current_location.isReturnSet();
                if (!isSet && i + 3 < tokens.size() && tokens[i + 3] == ";") {
                    current_location.setRcode(std::atoi(tokens[i + 1].c_str()));
                    current_location.setRurl(tokens[i + 2]);
                    current_location.setReturnSet(true);
                    i += 3;
                }
                else if (!isSet && i + 2 < tokens.size() && tokens[i + 2] == ";") {
                    current_location.setRcode(std::atoi(tokens[i + 1].c_str()));
                    if (current_location.getRcode() == 0)
                        current_location.setRurl(tokens[i + 1]);
                    else
                        current_location.setRcode(current_location.getRcode());
                    current_location.setReturnSet(true);
                    i += 2;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid return directive: " + tokens[i + 1]);
            }
            else if (token == "error_page") {
                if (i + 3 < tokens.size() && tokens[i + 3] == ";") {
                    checkErrorPage(tokens[i + 1], current_location);
                    std::map<int, std::string> error_pages = current_location.getErrorPages();
                    error_pages[std::atoi(tokens[i + 1].c_str())] = tokens[i + 2];
                    current_location.setErrorPages(error_pages);
                    i += 3;
                }
                else
                    throw std::runtime_error("Syntax error: Invalid error_page directive");
            }
            else
                throw std::runtime_error("Unknown directive in location block: " + token + " ");
        }
    }

    if (state != STATE_GLOBAL)
        throw std::runtime_error("Syntax error: Missing closing bracket '}'");
    this->validate();
    this->applyDefaults();
}

const std::vector<ServerConfig>& WebServConfig::getServers() const {
    return _servers;
}

void WebServConfig::validate() {
    for (size_t i = 0; i < _servers.size(); ++i)
        for (size_t j = i + 1; j < _servers.size(); ++j)
            if (_servers[i].getPort() == _servers[j].getPort() && _servers[i].getHost() == _servers[j].getHost()) {
                std::vector<std::string> names_i = _servers[i].getServerNames();
                std::vector<std::string> names_j = _servers[j].getServerNames();
                if (names_i.empty() && names_j.empty())
                    throw std::runtime_error("Config Error: Duplicate default servers on same port/host.");
                for (size_t k = 0; k < names_i.size(); ++k)
                    for (size_t l = 0; l < names_j.size(); ++l)
                        if (names_i[k] == names_j[l])
                            throw std::runtime_error("Config Error: Conflicting server name '" + names_i[k] + "' on same port.");
            }
}

void WebServConfig::applyDefaults() {
    for (size_t i = 0; i < _servers.size(); ++i) {
        if (_servers[i].getHost().empty())
            _servers[i].setHost("0.0.0.0");
        if (_servers[i].getPort().empty())
            _servers[i].setPort(std::vector<int>(1, 8080));
        if (_servers[i].getServerNames().empty()) {
            std::vector<std::string> default_names;
            default_names.push_back(""); 
            _servers[i].setServerNames(default_names);
        }
        if (_servers[i].isClientMaxBodySizeSet() == false)
            _servers[i].setClientMaxBodySize(1048576);
        else if (_servers[i].getClientMaxBodySize() <= 0)
            throw std::runtime_error("Config Error: Client max body size cant be a zero or less");
        if (_servers[i].getRoot().empty())
            _servers[i].setRoot("./www"); 
        std::vector<LocationConfig> locs = _servers[i].getLocations();
        for (size_t j = 0; j < locs.size(); ++j) {
            if (locs[j].getRoot().empty())
                locs[j].setRoot(_servers[i].getRoot());
            if (locs[j].getIndex().empty()) {
                std::vector<std::string> default_index;
                default_index.push_back("index.html");
                locs[j].setIndex(default_index);
            }
        }
        _servers[i].setLocations(locs);
    }
}
