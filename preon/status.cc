#include "error.h"
#include "status.h"
#include "utils.h"

#include <fstream>

namespace {

enum class State {
    none,
    master,
    execution_status,
    block_status_file,
    block_status_bits,
};

}

Status::Status(const std::string &filename) :
    m_filename(filename)
{
    m_master = false;
    m_execution_status = false;
}

bool Status::get_master() {
    return m_master;
}

void Status::set_master(bool master) {
    m_master = master;
}

bool Status::get_execution_status() {
    return m_execution_status;
}

void Status::set_execution_status(bool execution_status) {
    m_execution_status = execution_status;
}

void Status::add_file(const File &file) {
    m_file_blk_status[file.name];
    set_file(file, DONE);
}

void Status::set_file(const File &file, int state) {
    auto it = m_file_blk_status.find(file.name);
    if (it == m_file_blk_status.end())
        throw PE("file not in status");
    it->second.set_n_blocks(size_to_nblks(file.size), state);
}

void Status::reset_file(const std::string &filename) {
    auto it = m_file_blk_status.find(filename);
    if (it == m_file_blk_status.end())
        throw PE("filename not in status");

    it->second.reset();
}

void Status::set_block_status(const std::string &filename, int blk_id, int state) {
    auto it = m_file_blk_status.find(filename);
    if (it == m_file_blk_status.end())
        throw PE("file not in status");
    it->second.set_state(blk_id, state);
}

bool Status::file_has_block(const std::string &filename, int blk_id) {
    auto it = m_file_blk_status.find(filename);
    if (it == m_file_blk_status.end())
        throw PE("File does not exist");

    return it->second.get_state(blk_id) == DONE;
}

bool Status::file_is_finished(const std::string &filename) {
    auto it = m_file_blk_status.find(filename);
    if (it == m_file_blk_status.end())
        throw PE("File not in status");

    return it->second.finished();
}

int Status::file_get_n_blocks(const std::string &filename) {
    auto it = m_file_blk_status.find(filename);
    if (it == m_file_blk_status.end())
        throw PE("File not in status");

    return it->second.get_n_blocks();
}

int Status::file_claim_empty_block(const std::string &filename) {
    auto it = m_file_blk_status.find(filename);
    if (it == m_file_blk_status.end())
        throw PE("File does not exist");

    Blocks &blocks = it->second;
    int n_blocks = blocks.get_n_blocks();
    if (n_blocks == 0)
        return -1;

    int random_block = rand() % n_blocks;

    int i;
    for (i = random_block; i < n_blocks; i++) {
        if (blocks.get_state(i) == EMPTY) {
            blocks.set_state(i, DOWNLOADING);
            return i;
        }
    }

    for (i = 0; i < random_block; i++) {
        if (blocks.get_state(i) == EMPTY) {
            blocks.set_state(i, DOWNLOADING);
            return i;
        }
    }

    // No empty blocks
    return -1;
}

void Status::init(const std::vector<File> &files) {
    m_master = false;
    m_execution_status = false;

    for (const File &f: files) {
        int n_blocks = size_to_nblks(f.size);
        m_file_blk_status[f.name] = Blocks(n_blocks);
    }
}

bool Status::read(const std::vector<File> &files) {
    std::ifstream file(m_filename);
    if (!file.is_open())
        return false;

    State state = State::none;
    std::string status_filename;
    std::string line;
    int nline = 0;
    while(std::getline(file, line)) {
        nline++;

        line = strip(line);
        if (line.size() == 0)
            continue;

        // Header
        if (line[0] == '[') {
            if (line == "[master]")
                state = State::master;
            else if (line == "[block_status]")
                state = State::block_status_file;
            else if (line == "[execution_status]")
                state = State::execution_status;
            else
                throw PE("Invalid header line in " + PREON_STATUS_FILE +
                        " [line " + std::to_string(nline) + "]");
            continue;
        }

        if (state == State::master) {
            if (line == "true")
                m_master = true;
        }
        else if (state == State::execution_status) {
            if (line == "true")
                m_execution_status = true;
        }
        else if (state == State::block_status_file) {
            status_filename = line;
            state = State::block_status_bits;
        }
        else if (state == State::block_status_bits) {
            std::vector<int> status;
            status.reserve(line.size());
            for (char &c : line) {
                int value = (int)c - '0';

                if (value == DOWNLOADING)
                    value = EMPTY;

                status.push_back(value);
            }

            m_file_blk_status[status_filename].append_states(status);
        }
        else {  // Impies state == State::none
            throw PE("Invalid line in " + PREON_STATUS_FILE +
                    " [line " + STR(nline) + "]");
        }
    }

    file.close();

    check_file_consistency(files);

    return true;
}

void Status::write() {
    const std::string INDENT = "   ";

    std::ofstream file(m_filename);
    if (!file.is_open())
        throw PE("Failed to open status file");

    file << "[master]" << std::endl;
    file << INDENT << (m_master ? "true" : "false") << std::endl;
    file << std::endl;

    file << "[execution_status]" << std::endl;
    file << INDENT << (m_execution_status ? "true" : "false") << std::endl;
    file << std::endl;

    for (auto &f: m_file_blk_status) {
        file << "[block_status]" << std::endl;
        file << INDENT << f.first << std::endl;
        file << INDENT;
        for (int s: f.second)
            file << s;
        file << std::endl << std::endl;
    }
}

void Status::check_file_consistency(const std::vector<File> &files) {
    for (const File &file: files) {
        auto it = m_file_blk_status.find(file.name);
        if (it == m_file_blk_status.end()) {
            // file does not exist, add it
            m_file_blk_status[file.name] = Blocks(size_to_nblks(file.size));
            continue;
        }

        // If we done have the hash, we dont have the meta data
        // hence we skip this check. Setting the block size to 0
        // is not a good idea.
        if (file.hash.empty())
            continue;

        if (it->second.get_n_blocks() != size_to_nblks(file.size)) {
            warn("file: '" + file.name + "' had an invalid amount of blocks.");
            m_file_blk_status[file.name] = Blocks(size_to_nblks(file.size));
        }
    }

    if (m_file_blk_status.size() != files.size()) {
        warn("m_fileblk.size != files.size()");
        m_file_blk_status.clear();
        for (const File &file: files)
            m_file_blk_status[file.name] = Blocks(size_to_nblks(file.size));
    }
}
