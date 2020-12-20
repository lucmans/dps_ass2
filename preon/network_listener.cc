#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "error.h"
#include "network_listener.h"

NetworkListener::NetworkListener(unsigned short port) {
    m_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd == -1)
        throw PE_SYS("socket");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if(bind(m_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(m_fd);
        throw PE_SYS("bind");
    }

    if (listen(m_fd, 10) == -1) {
        close(m_fd);
        throw PE_SYS("listen");
    }
}

NetworkListener::~NetworkListener() {
    if (m_fd != -1)
        close(m_fd);
}

int NetworkListener::wait() {
    int fd;
    for (;;) {
        struct sockaddr_in caddr;
        socklen_t caddr_len = sizeof(caddr);
        fd = accept(m_fd, (struct sockaddr *)&caddr, &caddr_len);

        if (fd != -1)
            break;
        else
            warn(STR("accept(): ") + strerror(errno));
    }

    return fd;
}

