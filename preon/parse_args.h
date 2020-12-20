#ifndef __parse_args_h__
#define __parse_args_h__

#include <string>
#include <vector>

struct Args {
    bool create_job;
    bool work_job;

    std::vector<std::string> static_files;
    std::vector<std::string> dynamic_files;
    std::string exec_cmd;
    std::string job_id;

    bool n_workers_set;
    unsigned n_workers;
};

void parse_args(int argc, char *argv[], Args &args);

#endif //#ifndef __parse_args_h__
