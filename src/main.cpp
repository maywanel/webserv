#include "WebservConfig.hpp"

int main(int ac, char* av[]) {
    WebServConfig test;
    if (ac < 2) return 1;
    try {
        test.parse(av[1]);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}