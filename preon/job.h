#ifndef JOB_H
#define JOB_H

#include "blocks.h"
#include "preon_types.h"
#include "manifest.h"
#include "status.h"

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

class Job {
    public:
        Job(const std::string &_dir, const std::string &_id);

        std::string get_job_id();
        std::string get_job_dir();

        std::string get_manifest();

        void read_manifest();

        bool is_fishined(const std::string &filename);
        bool is_master();

        void write_block(const std::string &filename, int block_id,
                const std::vector<uint8_t> &data);
        bool read_block(const std::string &filename, int block_id,
                std::vector<uint8_t> &data);

        int claim_empty_block(const std::string &filename);

        void get_files(std::vector<File> &files);
        void reset_file(const std::string &filename);
        void hash_dynamic_files();
        void update_dynamic_metadata(const std::vector<File> &dynamic_metadata);

        std::string get_exec_cmd();
        void execution_finished();
        bool get_execution_finished();

    private:
        std::mutex lock;

        const std::string id;
        const std::string dir;

        Manifest manifest;
        Status   status;

        bool unsafe_is_fishined(const std::string &filename);
        void unsafe_write_block(const std::string &filename,
                int block_id, const std::vector<uint8_t> &data);
        bool unsafe_read_block(const std::string &filename,
                int block_id, std::vector<uint8_t> &data);

        void unsafe_hash_dynamic_files();
};


#endif  // JOB_H
