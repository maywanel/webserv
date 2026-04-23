// SessionManager.hpp
#pragma once
#include <map>
#include <string>
#include <cstdlib>
#include <sstream>
#include <ctime>

class SessionManager {
    private:
        static std::map<std::string, int> _sessions;
    public:
        SessionManager();
        ~SessionManager();
        bool isValidSession(const std::string& session_id);
        std::string createNewSession();
        int getVisits(const std::string& session_id);
        void incrementVisits(const std::string& session_id);
};