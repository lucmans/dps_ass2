#include <algorithm>
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

#include "error.h"
#include "tracker.h"
#include "utils.h"

namespace {

void verify_job_id(const std::string &job_id) {
    if (job_id == "idle")
        return;

    if (job_id.size() != 64)
        throw PE("Invalid job id length");

    for (int i = 0; i < 64; i++) {
        char c = job_id[i];
        if ((c < '0' || c > '9') && (c < 'a' || c > 'f'))
            throw PE("Invalid job id characters");
    }
}

}

Tracker::Tracker(const std::string &hostname, unsigned short port) {
    m_hostname = hostname;
    m_port = std::to_string(port);
}

void Tracker::inform_job(unsigned short port, const std::string &job_id) {
    verify_job_id(job_id);

    std::stringstream ss;
    ss << "INFORM " << port << " " << job_id << "\n";

    std::string resp;
    msg_tracker(resp, ss.str());

    if (resp != "OK\n")
        throw PE("Response not OK\nResponse: " + resp);
}

void Tracker::remove_job(unsigned short port, const std::string &job_id) {
    verify_job_id(job_id);

    std::stringstream ss;
    ss << "DELETE " << port << " " << job_id << "\n";

    std::string resp;
    msg_tracker(resp, ss.str());

    if (resp != "OK\n")
        throw PE("Response not OK");
}

PreonAddrList Tracker::query_job(const std::string &job_id) {
    verify_job_id(job_id);

    std::stringstream ss;
    ss << "QUERY " << job_id << "\n";

    std::string resp;
    msg_tracker(resp, ss.str());

    if (resp == "FAILED\n")
        throw PE("Response FAILED");

    PreonAddrList list;
    std::vector<std::string> strings;
    split(resp, strings, '\n');
    for (auto &s: strings) {
        if (s.empty())
            continue;

        size_t index = s.find(' ');
        if (index == std::string::npos)
            throw PE("Invalid IP:port string");

        int port;
        try {
            port = std::stoi(s.substr(index));
            if (port < 0 || port > 65535)
                throw PE("Invalid port");
        }
        catch (...) {
            throw PE("Invalid port");
        }
        list.push_back({s.substr(0, index), (unsigned short)port});
    }

    std::random_shuffle(list.begin(), list.end());

    return list;
}

int Tracker::connect_tracker() {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    struct addrinfo *result;
    int ret = getaddrinfo(m_hostname.c_str(), m_port.c_str(), &hints, &result);
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

    return fd;
}

void Tracker::msg_tracker(std::string &response, const std::string &msg) {
    int fd = connect_tracker();

    response.clear();
    try {
        send_tracker(fd, msg);
        recv_tracker(fd, response);
    }
    catch (PreonExcept &e) {
        close(fd);
        throw e;
    }

    close(fd);
}

void Tracker::send_tracker(int fd, const std::string &msg) {
    size_t bytes_send = 0;
    while (bytes_send != msg.size()) {
        ssize_t ret = send(fd, msg.c_str() + bytes_send, msg.size() - bytes_send, 0);

        if (ret == -1 && (errno != EINTR || errno != EAGAIN)) {
            throw PE_SYS("send");
        }
        else if (ret == 0) {
            throw PE("Stream closed unexpected");
        }

        bytes_send += ret;
    }
}

void Tracker::recv_tracker(int fd, std::string &response) {
    response.clear();

    const int size = 512;
    char buf[size];
    for (;;) {
        ssize_t ret = recv(fd, buf, size - 1, 0);

        if (ret == -1 && (errno != EINTR || errno != EAGAIN)) {
            throw PE_SYS("recv");
        }
        else if (ret == 0) {
            break;
        }

        buf[ret] = '\0';
        response += buf;
    }
}
