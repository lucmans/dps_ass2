#include <iostream>
#include <sys/socket.h>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <map>
#include <set>
#include <sstream>

#include "string_check.h"
#include "../preon/preon_types.h"

std::map<std::string, std::set<PreonAddr>> g_db;

bool handle_inform_msg(std::string ip_addr, std::string header) {
    std::stringstream ss(header);
    std::string msg;
    unsigned short port;
    std::string job_id;
    ss >> msg >> port >> job_id;

    PreonAddr pa = {ip_addr, port};
    g_db[job_id].insert(pa);

    return true;
}

bool handle_delete_msg(std::string ip_addr, std::string header) {
    std::stringstream ss(header);
    std::string msg;
    unsigned short port;
    std::string job_id;
    ss >> msg >> port >> job_id;

    PreonAddr pa = {ip_addr, port};
    g_db[job_id].erase(pa);

    return true;
}

bool handle_query_msg(PreonAddrList &list, std::string header) {
    std::stringstream ss(header);
    std::string msg;
    std::string job_id;
    ss >> msg >> job_id;

    auto &jobs = g_db[job_id];
    list.assign(jobs.begin(), jobs.end());

    return true;
}

void handle_connection(struct sockaddr_in *addr, int fd) {
    char ip_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ip_addr, INET_ADDRSTRLEN);

    // TODO check if local, if so return?

    char header[128];
    ssize_t header_size = recv(fd, header, sizeof(header) - 1, 0);
    if (header_size == -1)
        return;
    header[header_size] = '\0';

    if (is_inform_msg(header)) {
        handle_inform_msg(ip_addr, header);
        send(fd, "OK\n", 3, 0);
    }
    else if (is_delete_msg(header)) {
        handle_delete_msg(ip_addr, header);
        send(fd, "OK\n", 3, 0);
    }
    else if (is_query_msg(header)) {
        PreonAddrList list;
        handle_query_msg(list, header);
        std::stringstream ss;
        for (PreonAddr addr: list) {
            ss << addr.ip_addr << " " << addr.port << "\n";
        }
        std::string data = ss.str();
        send(fd, data.c_str(), data.size(), 0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " port" << std::endl;
        return EXIT_FAILURE;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(std::stoi(argv[1]));

    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        std::cout << "Error: Couldn't bind socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    listen(fd, 10);

    for (;;) {
        struct sockaddr_in caddr;
        socklen_t caddr_len = sizeof(caddr);
        int cfd = accept(fd, (struct sockaddr *)&caddr, &caddr_len);
        if (cfd == -1)
            continue;

        handle_connection(&caddr, cfd);
        close(cfd);
    }

    return 0;
}
