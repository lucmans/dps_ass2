#ifndef __tracker_h__
#define __tracker_h__

#include "preon_types.h"

#include <string>
#include <vector>

class Tracker {
    public:
        Tracker(const std::string &hostname, unsigned short port);

        void inform_job(unsigned short port, const std::string &job_id);
        void remove_job(unsigned short port, const std::string &job_id);
        PreonAddrList query_job(const std::string &job_id);

    private:
        std::string     m_hostname;
        std::string     m_port;

        int connect_tracker();
        void msg_tracker(std::string &response, const std::string &msg);
        void send_tracker(int fd, const std::string &msg);
        void recv_tracker(int fd, std::string &response);
};

#endif //#ifndef __tracker_h__
