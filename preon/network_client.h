#ifndef __network_client_h__
#define __network_client_h__

#include <string>
#include <cstdint>

#include "preon_types.h"
#include "program_state.h"

class NetworkClient {
    public:
        NetworkClient(const PreonAddr &addr, ProgramState *state);
        NetworkClient(int fd, ProgramState *state);
        ~NetworkClient();

        NetworkClient(const NetworkClient &) = delete;
        NetworkClient &operator=(const NetworkClient &) = delete;

        bool has_job(const std::string &job_id);
        bool get_block(const std::string &job_id, const std::string &file,
                int block_id, std::vector<uint8_t> &block);
        bool get_manifest(const std::string &job_id, std::string &manifest);
        bool get_dynamic_metadata(const std::string &job_id,
                std::vector<File> &dynamic_metadata);
        bool inform_job(const std::string &job_id);

        void wait();

    private:
        int m_fd;
        ProgramState *m_state;

        void send_msg(const std::string &msg);
        void send_block(const std::vector<uint8_t> &block);
        bool recv_msg(std::string &response);
        void recv_block(std::vector<uint8_t> &block, size_t size);
};

#endif //#ifndef __network_client_h__
