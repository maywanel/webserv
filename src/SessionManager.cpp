#include "SessionManager.hpp"

std::map<std::string, int> SessionManager::_sessions;

SessionManager::SessionManager() {}
SessionManager::~SessionManager() {}

bool SessionManager::isValidSession(const std::string& session_id) {
    return _sessions.find(session_id) != _sessions.end();
}

std::string SessionManager::createNewSession() {
    static unsigned int counter = 0;
    std::ostringstream oss;
    oss << "session_" << static_cast<unsigned long>(std::time(NULL))
        << "_" << counter++;
    std::string session_id = oss.str();
    _sessions[session_id] = 0;
    return session_id;
}

int SessionManager::getVisits(const std::string& session_id) {
    if (isValidSession(session_id))
        return _sessions[session_id];
    return -1;
}

void SessionManager::incrementVisits(const std::string& session_id) {
    if (isValidSession(session_id)) {
        _sessions[session_id]++;
    }
}
