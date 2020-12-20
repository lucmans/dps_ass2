#ifndef __manifest_h__
#define __manifest_h__

#include "preon_types.h"

#include <string>
#include <map>

class Manifest {
    public:
        Manifest(const std::string &filename);

        std::string get_text();
        bool get_text_empty();

        File get_file(const std::string &filename);
        void add_file(const File &filename);
        void get_files(std::vector<File> &files);

        std::string get_exec_cmd();
        void set_exec_cmd(const std::string &exec_cmd);

        void read();
        void write();

    private:
        const std::string           m_filename;

        std::string                 m_text;
        std::map<std::string, File> m_files;
        std::string                 m_exec_cmd;
};

#endif //#ifndef __manifest_h__
