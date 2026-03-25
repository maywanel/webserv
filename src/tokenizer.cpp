#include "WebservConfig.hpp"

bool isDelimiter(char c) {
    return (c == '{' || c == '}' || c == ';');
}

std::vector<std::string> WebServConfig::tokenizeConfig(const std::string& filename) {
    std::vector<std::string> tokens;
    std::ifstream file(filename.c_str());

    if (!file.is_open()) {
        std::cerr << "Error: Could not open config file: " << filename << std::endl;
        return tokens;
    }

    std::string line;
    while (std::getline(file, line)) {
        
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        std::string paddedLine;
        for (size_t i = 0; i < line.length(); ++i) {
            if (isDelimiter(line[i])) {
                paddedLine += ' ';
                paddedLine += line[i];
                paddedLine += ' ';
            } else {
                paddedLine += line[i];
            }
        }

        std::stringstream ss(paddedLine);
        std::string word;
        while (ss >> word) {
            tokens.push_back(word);
        }
    }

    file.close();
    return tokens;
}
