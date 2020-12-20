
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <cstring>


void parse_args(unsigned int argc, char* argv[], long &size, int &seed, std::string &filename) {
    for(unsigned int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-n") == 0 && argc > i + 1) {
            try {
                size = std::stol(argv[i + 1]);
            }
            catch(...) {
                std::cout << "Error: Incorrect size" << std::endl;
                exit(EXIT_FAILURE);
            }

            i += 1;  // Advance 1 extra arguments
        }
        else if(strcmp(argv[i], "-s") == 0 && argc > i + 1) {
            try {
                seed = std::stoi(argv[i + 1]);
            }
            catch(...) {
                std::cout << "Error: Incorrect seed" << std::endl;
                exit(EXIT_FAILURE);
            }

            i += 1;  // Advance 1 extra arguments
        }
        else if(strcmp(argv[i], "-f") == 0 && argc > i + 1) {
            filename = argv[i + 1];
            i += 1;
        }
        else {
            if(strcmp(argv[i], "-h") != 0 && strcmp(argv[i], "--help") != 0)
                std::cout << "Incorrect usage.\n" << std::endl;

            std::cout << "Flags:\n"
                      << "  -n <size>      - Set output file size\n"
                      << "  -s <seed>      - Set seed\n"
                      << "  -f <filename>  - Set output filename\n"
                      << "  -h             - Prints help\n"
                      << std::endl;
            exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char *argv[]) {
    // Defaults
    long size = 1024 * 1024 * 1024;  // 1 GiB
    int seed = 0;
    std::string filename = "out";

    parse_args(argc, argv, size, seed, filename);
    std::cout << "Generating random file with:\n"
              << "  Size = " << size << "\n"
              << "  Seed " << seed << "\n"
              << "  Filename " << filename << "\n"
              << std::endl;

    srand(seed);
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: Couldn't open file" << std::endl;
        exit(EXIT_FAILURE);
    }

    long written = 0;
    while (written < size) {
        std::string random = std::to_string(rand());
        file.write(random.c_str(), std::min((long)random.size(), size - written));

        written += random.size();
    }

    return EXIT_SUCCESS;
}
