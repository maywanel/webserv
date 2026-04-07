#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>
#include <deque>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <cctype>
#include <climits>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

enum ParserState {
    STATE_GLOBAL,
    STATE_SERVER,
    STATE_LOCATION
};

enum WildcardState {
    STATE_FULL,
    STATE_NONE,
    STATE_PRE,
    STATE_SUF,
    STATE_ERROR
};

class LocationConfig {
    private:
        std::string                 _path;
        std::string                 _root;
        std::vector<std::string>    _index;
        bool                        _autoindex;
        bool                        _autoindex_set;
        std::vector<std::string>    _methods;
        int                         _rcode;
        std::string                 _rurl;
        bool                        _return_set;
        std::vector<std::string>    _cgi_ext;
        std::vector<std::string>    _cgi_path;
        std::string                 _upload_dir;
        std::map<int, std::string>  _error_pages;
    public:
        LocationConfig();
        const std::string& getPath() const;
        void setPath(const std::string& value);
        const std::string& getRoot() const;
        void setRoot(const std::string& value);
        const std::vector<std::string>& getIndex() const;
        void setIndex(const std::vector<std::string>& value);
        bool getAutoindex() const;
        void setAutoindex(bool value);
        bool isAutoindexSet() const;
        void setAutoindexSet(bool autoindex_set);
        const std::vector<std::string>& getMethods() const;
        void setMethods(const std::vector<std::string>& value);
        int getRcode() const;
        void setRcode(int value);
        const std::string& getRurl() const;
        void setRurl(const std::string& value);
        const std::vector<std::string>& getCgiExt() const;
        void setCgiExt(const std::vector<std::string>& value);
        const std::vector<std::string>& getCgiPath() const;
        void setCgiPath(const std::vector<std::string>& value);
        const std::string& getUploadDir() const;
        void setUploadDir(const std::string& value);
        bool isReturnSet() const;
        void setReturnSet(bool return_set);
        const std::map<int, std::string>& getErrorPages() const;
        void setErrorPages(const std::map<int, std::string>& error_pages);
};
