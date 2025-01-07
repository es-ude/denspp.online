#include "processing.h"
#include <iostream>

int main(int argc, char *argv[]) {
    try {
        std::string config_file_path = (argc >= 2) ? argv[1] : "../../config/default.yaml";
        std::cout << "Using Config: " << config_file_path << std::endl;

        Processing processing(config_file_path);
        processing.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}