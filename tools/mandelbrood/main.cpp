
#include "lodepng.h"

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <vector>
#include <string>


struct Domain {
    double r_min;
    double r_max;
    double i_min;
    double i_max;
};

struct Resolution {
    unsigned int width;
    unsigned int height;
};


int escape_iteration(const double c[2], const int n_max) {
    double z[2] = {0, 0};
    double z_squared[2] = {0, 0};

    // Escape iteration loop
    int n = 0;
    for(; n < n_max && z_squared[0] + z_squared[1] <= 4.0; n++) {
        z[1] = z[0] * z[1] * 2.0;
        z[0] = z_squared[0] - z_squared[1];

        z[0] += c[0];
        z[1] += c[1];

        z_squared[0] = z[0] * z[0];
        z_squared[1] = z[1] * z[1];
    }

    return n;
}

uint32_t color(const int n, const int n_max) {
    uint32_t rgba = 0;
    const double t = n / (double)n_max;
    // const int t = n % 256;

    rgba |= (uint8_t)(9 * (1 - t) * t * t * t * 255) << 24;  // r
    rgba |= (uint8_t)(15 * (1 - t) * (1 - t) * t * t * 255) << 16;  // g
    rgba |= (uint8_t)(8.5 * (1 - t) * (1 - t) * (1 - t) * t * 255) << 8;  // b

    return rgba;
}

void mandelbrot(const Domain &dom, const Resolution &res, const int n_max, std::vector<uint32_t> &pixels) {
    const double pixel_size = (dom.r_max - dom.r_min) / (double)res.width;

    double c[2];
    for(unsigned int y = 0; y < res.height; y++) {
        c[1] = dom.i_max - (y * pixel_size);

        for(unsigned int x = 0; x < res.width; x++) {
            c[0] = dom.r_min + (x * pixel_size);

            int escape = escape_iteration(c, n_max);
            pixels[y * res.width + x] = color(escape, n_max);
        }
    }
}


void parse_args(unsigned int argc, char* argv[], Domain &dom, Resolution &res, int &n_max, std::string &filename) {
    for(unsigned int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-d") == 0 && argc > i + 4) {
            try {
                dom.r_min = std::stod(argv[i + 1]);
                dom.r_max = std::stod(argv[i + 2]);
                dom.i_min = std::stod(argv[i + 3]);
                dom.i_max = std::stod(argv[i + 4]);
            }
            catch(...) {
                std::cout << "Error: Incorrect value in domain" << std::endl;
                exit(EXIT_FAILURE);
            }

            i += 4;  // Advance 4 extra arguments
        }
        else if(strcmp(argv[i], "-r") == 0 && argc > i + 2) {
            try {
                res.width = std::stoi(argv[i + 1]);
                res.height = std::stoi(argv[i + 2]);
            }
            catch(...) {
                std::cout << "Error: Incorrect width or height" << std::endl;
                exit(EXIT_FAILURE);
            }

            i += 2;  // Advance 2 extra arguments
        }
        else if(strcmp(argv[i], "-n") == 0 && argc > i + 1) {
            try {
                n_max = std::stoi(argv[i + 1]);
            }
            catch(...) {
                std::cout << "Error: Incorrect n_max" << std::endl;
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
                      << "  -d <r_min> <r_max> <i_min> <i_max>   - Set domain\n"
                      << "  -r <x> <y>                           - Set resolution\n"
                      << "  -n <n_max>                           - Set maximum iterations\n"
                      << "  -f <filename>                        - Set output filename\n"
                      << "  -h                                   - Prints help\n"
                      << std::endl;
            exit(EXIT_SUCCESS);
        }
    }
}

void write_blob(const std::string &filename, const std::vector<uint32_t> &data) {
    FILE *fp = fopen(filename.c_str(), "wb");
    if(fp == NULL) {
        std::cout << "Error: Couldn't open file" << std::endl;
        exit(EXIT_FAILURE);
    }

    unsigned int written = 0;
    while(written < data.size())
        written += fwrite(data.data() + written, sizeof(uint32_t), data.size() - written, fp);

    fclose(fp);
}

void cast_vector(std::vector<unsigned char> &out, const std::vector<uint32_t> &in) {
    for(const uint32_t &p : in) {
        out.push_back((p & (0xff << 24)) >> 24);
        out.push_back((p & (0xff << 16)) >> 16);
        out.push_back((p & (0xff << 8)) >> 8);
        out.push_back(255);
    }
}

int main(int argc, char *argv[]) {
    // Defaults
    Domain dom = {-2, 1, -1.125, 1.125};
    Resolution res = {800, 600};
    int n_max = 256;
    std::string filename = "out";

    parse_args(argc, argv, dom, res, n_max, filename);
    std::cout << "Generating fractal with:\n"
              << "  Domain r = [" << dom.r_min << ", " << dom.r_max << "] and i = [" << dom.i_min << ", " << dom.i_max << "]\n"
              << "  Resolution " << res.width << "x" << res.height << "\n"
              << "  N_max " << n_max << "\n"
              << "  Filename " << filename << "\n"
              << std::endl;

    // Calculate fractal
    std::vector<uint32_t> pixels;
    pixels.resize(res.width * res.height);
    mandelbrot(dom, res, n_max, pixels);

    // Output RBGA blob
    write_blob(filename + ".blob", pixels);

    // Output png
    std::vector<unsigned char> kut_lodepng;
    cast_vector(kut_lodepng, pixels);
    unsigned int error = lodepng::encode(filename + ".png", kut_lodepng, res.width, res.height);
    if(error) {
        std::cout << "Encoder error: " << error << ": "<< lodepng_error_text(error) << std::endl;
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
