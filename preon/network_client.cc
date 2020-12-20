#include <iostream>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <sstream>
#include <vector>
#include <sys/stat.h>

#include "network_client.h"
#include "sha256.h"
#include "error.h"
#include "utils.h"
#include "tracker.h"

namespace {

std::string block_to_str(const std::vector<uint8_t> &block) {
    std::string str;

    str.reserve(block.size());
    for (uint8_t b: block) {
        if (b == 0)
            throw PE("Block can nog contain a 0 value");
        str += (char)b;
    }

    return str;
}

std::vector<uint8_t> str_to_block(const std::string &str) {
    std::vector<uint8_t> block;

    block.reserve(str.size());
    for (char c: str) {
        block.push_back((uint8_t)c);
    }

    return block;
}

}

NetworkClient::NetworkClient(const PreonAddr &addr, ProgramState *state) {
    m_state = state;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    struct addrinfo *result;
    int ret = getaddrinfo(addr.ip_addr.c_str(), std::to_string(addr.port).c_str(), &hints, &result);
    if (ret != 0) {
        throw PE_SYS("getaddrinfo");
    }

    int fd;
    struct addrinfo *rp;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1)
            continue;

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;

        close(fd);
    }

    freeaddrinfo(result);

    if (rp == NULL)
        throw PE("Could not resolve address");

    m_fd = fd;
}

NetworkClient::NetworkClient(int fd, ProgramState *state) {
    m_state = state;
    m_fd = fd;
}

NetworkClient::~NetworkClient() {
    if (m_fd != -1)
        close(m_fd);
    m_fd = -1;
}

bool NetworkClient::has_job(const std::string &job_id) {
    std::stringstream ss;
    ss << "HAS_JOB " << job_id << "\n";
    send_msg(ss.str());

    std::string response;
    recv_msg(response);
    return response == "TRUE\n";
}

bool NetworkClient::get_block(const std::string &job_id, const std::string &file,
        int block_id, std::vector<uint8_t> &block) {
    std::stringstream ss;
    ss << "GET_BLOCK " << job_id << " " << file << " " << block_id << "\n";
    send_msg(ss.str());

    std::string response;
    recv_msg(response);
    if (response == "FALSE\n")
        return false;

    size_t block_size = std::stoi(response);
    block.clear();
    recv_block(block, block_size);

    return true;
}

bool NetworkClient::get_manifest(const std::string &job_id, std::string &manifest) {
    std::stringstream ss;
    ss << "GET_MANIFEST " << job_id << "\n";
    send_msg(ss.str());

    std::string response;
    recv_msg(response);

    if (response == "FALSE\n")
        return false;

    size_t block_size = std::stoi(response);
    std::vector<uint8_t> block;
    recv_block(block, block_size);

    // verify the response
    SHA256_CTX ctx;
    BYTE hash[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const BYTE *)block.data(), block.size());
    sha256_final(&ctx, hash);
    if (sha256_to_str(hash) != job_id) {
        std::cout << "klopt niet: " << sha256_to_str(hash) << std::endl;
        return false;
    }

    manifest.assign(block.begin(), block.end());
    return true;
}

bool NetworkClient::get_dynamic_metadata(const std::string &job_id,
        std::vector<File> &dynamic_metadata) {
    std::stringstream ss;
    ss << "GET_DYNAMIC_METADATA " << job_id << "\n";
    send_msg(ss.str());

    std::string response;
    recv_msg(response);
    if (response == "FALSE\n")
        return false;

    size_t block_size = std::stoi(response);
    std::vector<uint8_t> block;
    recv_block(block, block_size);

    std::stringstream block_ss(block_to_str(block));
    dynamic_metadata.clear();
    std::string line;
    while (std::getline(block_ss, line, '\n')) {
        if (line == "")
            continue;

        std::stringstream line_ss(line);
        std::string filename, hash;
        uint64_t size;
        line_ss >> filename >> hash >> size;

        dynamic_metadata.push_back({filename, hash, size, true});
    }

    return true;
}

bool NetworkClient::inform_job(const std::string &job_id) {
    std::stringstream ss;
    ss << "INFORM_JOB " << job_id << "\n";
    send_msg(ss.str());

    std::string response;
    recv_msg(response);
    return response == "TRUE\n";
}

void NetworkClient::wait() {
    for (;;) {
        std::string header;
        if (! recv_msg(header))
            break; //connection closed

        std::stringstream ss(header);
        std::string type;
        ss >> type;
        if (type == "HAS_JOB") {
            std::string job_id;
            ss >> job_id;
            std::cerr << m_state->get_job(job_id);
            send_msg(m_state->get_job(job_id) ? "TRUE\n" : "FALSE\n");
        }
        else if (type == "GET_MANIFEST") {
            std::string job_id;
            ss >> job_id;

            Job *job = m_state->get_job(job_id);
            if (job == nullptr)  {
                send_msg("FALSE\n");
                continue;
            }

            std::string manifest = job->get_manifest();
            if (manifest.empty()) {
                send_msg("FALSE\n");
                continue;
            }

            send_msg(std::to_string(manifest.size()) + "\n");

            std::vector<uint8_t> buf(manifest.begin(), manifest.end());
            send_block(buf);
        }
        else if (type == "GET_BLOCK") {
            std::string job_id, file;
            int block_id;
            ss >> job_id >> file >> block_id;

            Job *job = m_state->get_job(job_id);
            if (job == nullptr) {
                send_msg("FALSE\n");
                continue;
            }

            std::vector<uint8_t> block;
            if (!job->read_block(file, block_id, block)) {
                send_msg("FALSE\n");
                continue;
            }

            send_msg(std::to_string(block.size()) + "\n");
            send_block(block);
        }
        else if (type == "GET_DYNAMIC_METADATA") {
            std::string job_id;
            ss >> job_id;

            Job *job = m_state->get_job(job_id);
            if (job == nullptr) {
                send_msg("FALSE\n");
                continue;
            }

            if (!job->get_execution_finished()) {
                send_msg("FALSE\n");
                continue;
            }

            std::vector<File> files;
            job->get_files(files);

            std::stringstream ss;
            for (const File &file: files) {
                if (!file.dynamic)
                    continue;

                ss << file.name << " " << file.hash << " " << file.size << "\n";
            }

            std::vector<uint8_t> block = str_to_block(ss.str());

            send_msg(std::to_string(block.size()) + "\n");
            send_block(block);
        }
        else if (type == "INFORM_JOB") {
            std::string job_id;
            ss >> job_id;

            if (!is_job_hash(job_id)) {
                send_msg("FALSE\n");
                continue;
            }

            if (m_state->get_job(job_id)) {
                send_msg("FALSE\n");
                continue;
            }

            if (m_state->get_n_idle_workers() == 0) {
                send_msg("FALSE\n");
                continue;
            }

            Config *config = m_state->get_config();
            std::string download_dir = config->get_download_folder();
            std::string job_dir = download_dir + "/" + job_id;

            if (mkdir(job_dir.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1) {
                if (errno != EEXIST)
                    warn(STR("mkdir(): ") + strerror(errno));
                send_msg("FALSE\n");
                continue;
            }

            m_state->add_job(download_dir, job_id);

            Tracker tracker(config->get_tracker_url(), config->get_tracker_port());
            tracker.inform_job(config->get_listen_port(), job_id);

            info("accepted new job: " + job_id);

            send_msg("TRUE\n");
        }
    }
}

void NetworkClient::send_msg(const std::string &msg) {
    ssize_t res = send(m_fd, msg.c_str(), msg.size(), 0);
    if (res == -1)
        throw PE_SYS("send");
    else if (static_cast<size_t>(res) != msg.size())
        throw PE("Failed to send complete message");
}

void NetworkClient::send_block(const std::vector<uint8_t> &block) {
    size_t bytes_send = 0;
    while (bytes_send != block.size()) {
        ssize_t res = send(m_fd, block.data() + bytes_send, block.size() - bytes_send, 0);
        if (res == -1)
            throw PE_SYS("send");
        else if (res == 0)
            throw PE("Stream ended unexpected");

        bytes_send += res;
    }
}

bool NetworkClient::recv_msg(std::string &response) {
    response.clear();

    char c;
    do {
        ssize_t ret = recv(m_fd, &c, 1, 0);
        if (ret == -1)
            throw PE_SYS("recv");
        else if (ret == 0) {
            response.clear();
            return false;
        }
        else if (ret != 1)
            throw PE("You fucked up real bad");
        response += c;
    } while (c != '\n');

    return true;
}

void NetworkClient::recv_block(std::vector<uint8_t> &block, size_t size) {
    block.clear();

    while (block.size() != size) {
        char buf[1024];
        ssize_t ret = recv(m_fd, &buf, std::min(sizeof(buf), size - block.size()), 0);
        if (ret == -1)
            throw PE_SYS("recv");
        else if (ret == 0)
            throw PE("Network stream ended unexpected");

        block.insert(block.end(), buf, buf + ret);
    }
}
