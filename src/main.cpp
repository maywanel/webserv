#include "ServerManager.hpp"

int main(int ac, char* av[]) {
    WebServConfig test;
    if (ac > 2) return 1;
    try {
        if (ac < 2)
            test.parse("conf/default.conf");
        else
            test.parse(av[1]);
        ServerManager server(test);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}