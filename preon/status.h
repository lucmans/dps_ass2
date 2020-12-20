#ifndef __status_h__
#define __status_h__

#include "blocks.h"
#include "preon_types.h"

#include <vector>
#include <string>
#include <map>

class Status {
    public:
        Status(const std::string &filename);

        bool get_master();
        void set_master(bool master);

        bool get_execution_status();
        void set_execution_status(bool execution_status);

        void add_file(const File &file);
        void set_file(const File &file, int state = EMPTY);
        void reset_file(const std::string &filename);
        void set_block_status(const std::string &filename, int blk_id, int state);
        bool file_has_block(const std::string &filename, int blk_id);
        bool file_is_finished(const std::string &filename);
        int file_get_n_blocks(const std::string &filename);
        int file_claim_empty_block(const std::string &filename);

        void init(const std::vector<File> &files);
        bool read(const std::vector<File> &files);
        void write();

    private:
        const std::string   m_filename;

        bool                m_master;
        bool                m_execution_status;
        std::map<std::string, Blocks> m_file_blk_status;

        void check_file_consistency(const std::vector<File> &files);
};

#endif //#ifndef __status_h__
