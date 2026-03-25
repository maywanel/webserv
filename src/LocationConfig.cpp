#include "LocationConfig.hpp"

LocationConfig::LocationConfig() : _autoindex(false), _rcode(0) {}

const std::string& LocationConfig::getPath() const {
    return _path;
}
void LocationConfig::setPath(const std::string& value) {
    this->_path = value;
}

const std::string& LocationConfig::getRoot() const {
    return _root;
}
void LocationConfig::setRoot(const std::string& value) {
    this->_root = value;
}

const std::vector<std::string>& LocationConfig::getIndex() const {
    return _index;
}
void LocationConfig::setIndex(const std::vector<std::string>& value) {
    this->_index = value;
}

bool LocationConfig::getAutoindex() const {
    return _autoindex;
}
void LocationConfig::setAutoindex(bool value) {
    this->_autoindex = value;
}

const std::vector<std::string>& LocationConfig::getMethods() const {
    return _methods;
}
void LocationConfig::setMethods(const std::vector<std::string>& value) {
    this->_methods = value;
}

int LocationConfig::getRcode() const {
    return _rcode;
}
void LocationConfig::setRcode(int value) {
    this->_rcode = value;
}

const std::string& LocationConfig::getRurl() const {
    return _rurl;
}
void LocationConfig::setRurl(const std::string& value) {
    this->_rurl = value;
}

const std::vector<std::string>& LocationConfig::getCgiExt() const {
    return _cgi_ext;
}
void LocationConfig::setCgiExt(const std::vector<std::string>& value) {
    this->_cgi_ext = value;
}

const std::vector<std::string>& LocationConfig::getCgiPath() const {
    return _cgi_path;
}
void LocationConfig::setCgiPath(const std::vector<std::string>& value) {
    this->_cgi_path = value;
}

const std::string& LocationConfig::getUploadDir() const {
    return _upload_dir;
}
void LocationConfig::setUploadDir(const std::string& value) {
    this->_upload_dir = value;
}
