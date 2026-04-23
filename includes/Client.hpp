#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <fstream>
#include <sys/wait.h>
#include "ServerConfig.hpp"
#include "HttpRequest.hpp"
#include "SessionManager.hpp"

enum ClientState {
    STATE_READING_HEADERS,
    STATE_READING_BODY,
    STATE_PROCESSING,
    STATE_SENDING_RESPONSE,
    STATE_CGI_READING,
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
        SessionManager              _session_manager;
        pid_t                       _cgi_pid;
        int                         _cgi_write_fd;
        int                         _cgi_read_fd;
        std::string                 _cgi_buffer;
        bool                        _is_chunked;
    public:
        Client();
        Client(int fd, const std::vector<ServerConfig>& virtual_servers);
        Client(const Client& other);
        Client& operator=(const Client& other);
        ~Client();
        void                        appendRequestData(const char* data, ssize_t len);
        bool                        isHeaderComplete() const;
        void                        updateActivityTime();
        int                         getFd() const;
        ClientState                 getState() const;
        pid_t                       getCgiPid() const;
        int                         getCgiWriteFd() const;
        int                         getCgiReadFd() const;
        void                        setState(ClientState state);
        const std::string&          getRequestBuffer() const;
        const std::string&          getResponseBuffer() const;
        void                        appendResponseData(const char* data, ssize_t len);
        const ServerConfig*         getMatchedServer() const;
        void                        setMatchedServer(const ServerConfig* server);
        time_t                      getLastActivityTime() const;
        bool                        isTimedOut(time_t current_time, time_t timeout_seconds) const;
        std::string                 extractBodyFromBuffer(const std::string& buffer);
        size_t                      parseContentLength(const std::string& buffer);
        std::string                 resolveLocation();
        int                         checkPath(const std::string& path);
        std::string                 getFinalPath(std::string raw_path);
        void                        generateResponse(const std::string& target_path, ErrorStatus error);
        std::string                 getMimeType(const std::string& target_path);
        void                        generateErrorResponse(int error);
        std::string                 generateAutoindex(const std::string& target_path, const std::string& uri);
        std::string                 Multipartextraction(const std::string& temp_file, const std::string& final_file, const std::string& boundary);
        std::string                 extractSessionId(const std::string& cookie_header);
        bool                        executeCgi(const std::string& script_path);
        std::vector<std::string>    buildCgiEnv(const std::string& script_path);
        void                        appendCgiOutput(const char* data, ssize_t len);
        void                        parseCgiResponse();
        std::string                 decodeChunked(const std::string& chunked_body);
};

