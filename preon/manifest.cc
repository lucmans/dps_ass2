#include "consts.h"
#include "error.h"
#include "manifest.h"
#include "utils.h"

#include <sstream>
#include <fstream>

enum class State {
    none,
    files,
    exec,
    dynamic,
    deps,
};

Manifest::Manifest(const std::string &filename) :
    m_filename(filename) {}

std::string Manifest::get_text() {
    return m_text;
}

bool Manifest::get_text_empty() {
    return m_text.empty();
}

File Manifest::get_file(const std::string &filename) {
    auto it = m_files.find(filename);
    if (it == m_files.end())
        throw PE("File does not exist");
    return it->second;
}

void Manifest::add_file(const File &file) {
    m_files[file.name] = file;
}

void Manifest::get_files(std::vector<File> &files) {
    files.clear();
    for (const auto &pair: m_files)
        files.push_back(pair.second);
}

std::string Manifest::get_exec_cmd() {
    return m_exec_cmd;
}

void Manifest::set_exec_cmd(const std::string &exec_cmd) {
    m_exec_cmd = exec_cmd;
}

void Manifest::read() {
    if (m_text != "")
        throw PE("Manifest already read");

    m_text = file_to_str(m_filename);

    State state = State::none;
    std::string line;
    int nline = 0;
    std::stringstream ss(m_text);
    while(std::getline(ss, line)) {
        nline++;

        line = strip(line);
        if (line.size() == 0)
            continue;

        // Header
        if (line[0] == '[') {
            if (line == "[files]")
                state = State::files;
            else if (line == "[exec]")
                state = State::exec;
            else if (line == "[dynamic]")
                state = State::dynamic;
            else if (line == "[deps]")
                state = State::deps;
            else
                throw PE("Invalid header line in " + PREON_MANIFEST_FILE +
                        " [line " + STR(nline) + "]");
            continue;
        }

        if (state == State::files) {
            std::vector<std::string> strings;
            split(line, strings, ' ');
            if (strings.size() < 3)
                throw PE("Field missing in file line in " + PREON_MANIFEST_FILE +
                        " [line " + STR(nline) + "]");

            File file;
            file.hash = strings[strings.size() - 1];

            try {
                file.size = std::stol(strings[strings.size() - 2]);
            }
            catch (...) {
                throw PE("Invalid size in " + PREON_MANIFEST_FILE +
                        " [line " + STR(nline) + "]");
            }

            strings.erase(strings.end() - 2, strings.end());
            file.name = strings[0];
            for (unsigned int i = 1; i < strings.size(); i++)
                file.name += ' ' + strings[i];
            file.dynamic = false;

            m_files[file.name] = file;
        }
        else if (state == State::exec) {
            if (m_exec_cmd.size() != 0)
                throw PE("More than 1 exec line in " + PREON_MANIFEST_FILE +
                        " [line " + STR(nline) + "]");

            m_exec_cmd = line;
        }
        else if (state == State::dynamic) {
            std::string filename = strip(line);
            m_files[filename] = {filename, "", 0, true};
        }
        else if (state == State::deps) {
            warn("Dependencies not supported");
        }
        else {  // Impies state == State::none
            throw PE("Invalid line in " + PREON_MANIFEST_FILE +
                    " [line " + STR(nline) + "]");
        }
    }
}

void Manifest::write() {
    const std::string INDENT = "    ";

    std::ofstream out(m_filename);
    if (!out.is_open())
        throw PE("Couldn't open " + m_filename + " for writing");

    out << "[files]" << std::endl;
    for (auto &file : m_files) {
        if (!file.second.dynamic)
            out << INDENT << file.second.name << " " << file.second.size
                << " " << file.second.hash << std::endl;
    }
    out << std::endl;

    out << "[exec]" << std::endl
        << INDENT  << m_exec_cmd << std::endl
        << std::endl;

    out << "[dynamic]\n";
    for (auto &file : m_files) {
        if (file.second.dynamic)
            out << INDENT << file.second.name << std::endl;
    }
}

