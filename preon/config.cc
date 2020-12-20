#include <iostream>
#include <exception>
#include <fstream>
#include <limits>

#include "config.h"
#include "error.h"
#include "utils.h"

Config::Config(const std::string &config_filename) {
    m_tracker_url   = "";
    m_tracker_port  = 8000;
    m_listen_port   = 42069;
    m_download_folder = "/tmp/preon/";
    m_n_workers     = 8;

    load_file(config_filename);

    if (m_tracker_url.empty())
        throw PE("Tracker URL not set");
}

void Config::load_file(const std::string &filename) {
    std::ifstream file(filename);

    if (!file.is_open())
        throw PE("Failed to open config file");

    std::string line;
    while (std::getline(file, line)) {
        size_t sep = line.find('=');
        if (sep == std::string::npos)
            continue;

        if (sep == line.size() - 1)
            continue;

        std::string key(line.begin(), line.begin() + sep);
        std::string value(line.begin() + sep + 1, line.end());
        if (value.back() == '\n')
            value.erase(value.end() - 1);

        if (key == "tracker_url") {
            m_tracker_url = value;
        }
        else if (key == "tracker_port") {
            m_tracker_port = str_to_port(value);
        }
        else if (key == "listen_port") {
            m_listen_port = str_to_port(value);
        }
        else if (key == "download_folder") {
            m_download_folder = value;
        }
        else if (key == "n_workers") {
            m_n_workers = str_to_unsigned(value);
        }
        else {
            warn("ignoring key '" + key + "'");
        }
    }
}
