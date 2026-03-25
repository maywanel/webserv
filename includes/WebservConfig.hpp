#pragma once

#include "ServerConfig.hpp"

enum ParserState {
    STATE_GLOBAL,
    STATE_SERVER,
    STATE_LOCATION
};

class WebServConfig {
    private:
        std::vector<ServerConfig> _servers;
    public:
        WebServConfig();
        ~WebServConfig();
        std::vector<std::string> tokenizeConfig(const std::string& filename);
        void parse(const std::string& filename);
        const std::vector<ServerConfig>& getServers() const;
        void validate();
        void applyDefaults();
};