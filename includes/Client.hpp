#pragma once
#include <string>
#include <vector>
#include <ctime>
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"

enum ClientState {
    STATE_READING_HEADERS,
    STATE_READING_BODY,
    STATE_PROCESSING,
    STATE_SENDING_RESPONSE,
    STATE_DISCONNECT
};

enum PathType {
    PATH_IS_FILE = 0,
    PATH_IS_DIR = 1,
    PATH_NOT_FOUND = 2,
    PATH_UNKNOWN = -1
};

class Client {
    private:
        int                         _fd;
        std::vector<ServerConfig>   _virtual_servers;
        const ServerConfig*         _matched_server;
        std::string                 _request_buffer;
        std::string                 _response_buffer;
        ClientState                 _state;
        time_t                      _last_activity;
        std::string                 _temp_filename;
        std::ofstream               _body_file;
        size_t                      _bytes_received;
        size_t                      _content_length;
        const LocationConfig*       _matched_location;
        HttpRequest                 _request;
    public:
        Client();
        Client(int fd, const std::vector<ServerConfig>& virtual_servers);
        Client(const Client& other);
        Client& operator=(const Client& other);
        ~Client();
        void                    appendRequestData(const char* data, ssize_t len);
        bool                    isHeaderComplete() const;
        void                    updateActivityTime();
        int                     getFd() const;
        ClientState             getState() const;
        void                    setState(ClientState state);
        const std::string&      getRequestBuffer() const;
        const std::string&      getResponseBuffer() const;
        void                    appendResponseData(const char* data, ssize_t len);
        const ServerConfig*     getMatchedServer() const;
        void                    setMatchedServer(const ServerConfig* server);
        time_t                  getLastActivityTime() const;
        bool                    isTimedOut(time_t current_time, time_t timeout_seconds) const;
        std::string             extractBodyFromBuffer(const std::string& buffer);
        size_t                  parseContentLength(const std::string& buffer);
        std::string             resolveLocation();
        int                     checkPath(const std::string& path);
        std::string             getFinalPath(std::string raw_path);
        void                    generateResponse(const std::string& target_path, ErrorStatus error);
        std::string             getMimeType(const std::string& target_path);
        void                    generateErrorResponse(int error);
};

