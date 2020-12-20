#include "error.h"
#include "parse_args.h"
#include "utils.h"

#include <algorithm>
#include <string>
#include <vector>
#include <iostream>

namespace {

enum State {
    NONE, // initial state

    STATIC_FILES,
    STATIC_FILES_SET,
    EXEC_CMD,
    EXEC_CMD_SET,
    DYNAMIC_FILES,
    DYNAMIC_FILES_SET,

    JOB_ID,
    JOB_ID_SET,

    N_WORKERS,
};

void print_help(const char *argv0) {
    std::cout
        << "Flags:"                                         << std::endl
        << "  -h, --help            Print this message"     << std::endl
        << "  -c, --create          Create new job"         << std::endl
        << "  -e, --exec_cmd        Specify exec command"   << std::endl
        << "  -d, --dynamic         Specify dynamic files"  << std::endl
        << "  -j, --job             Add existing job"       << std::endl
        << "  -n, --n_workers_set   Set number of workers"  << std::endl
        << std::endl
        << "Examples:"                                  << std::endl
        << " " << argv0 << " -c file0 ... file_n"       << std::endl
        << " " << argv0 << " -c file0 ... file_n -e file_i -d dfile0 ... dfile_n"
        << std::endl
        << " " << argv0 << " -j job_id"                 << std::endl;

}

void print_help_and_exit(const char *argv0) {
    print_help(argv0);
    exit(EXIT_FAILURE);
}

bool argcmp(const char *src, const char *a, const char *b) {
    return std::string(src) == a || std::string(src) == b;
}

}

void parse_args(int argc, char *argv[], Args &args) {
    // at most 1 option will be set
    args.create_job = false;
    args.work_job = false;
    args.static_files.clear();
    args.dynamic_files.clear();
    args.exec_cmd = "";
    args.job_id = "";
    args.n_workers_set = false;
    args.n_workers_set = 0;

    State state = NONE;
    for (int i = 1; i < argc; i++) {
        bool is_opt = argv[i][0] == '-';

        if (argcmp(argv[i], "-h", "--help") && state == NONE) {
            print_help(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else if (argcmp(argv[i], "-c", "--create") && state == NONE) {
            args.create_job = true;
            state = STATIC_FILES;
        }
        else if (argcmp(argv[i], "-j", "--job") && state == NONE) {
            args.work_job = true;
            state = JOB_ID;
        }
        else if (argcmp(argv[i], "-e", "--exec_cmd") && state == STATIC_FILES_SET) {
            state = EXEC_CMD;
        }
        else if (argcmp(argv[i], "-d", "--dynamic") && state == EXEC_CMD_SET) {
            state = DYNAMIC_FILES;
        }
        else if (argcmp(argv[i], "-n", "--n_workers") && state == NONE && !args.n_workers_set) {
            state = N_WORKERS;
        }
        else if (!is_opt && (state == STATIC_FILES || state == STATIC_FILES_SET)) {
            args.static_files.push_back(argv[i]);
            state = STATIC_FILES_SET;
        }
        else if (!is_opt && state == EXEC_CMD) {
            args.exec_cmd = argv[i];
            state = EXEC_CMD_SET;
        }
        else if (!is_opt && (state == DYNAMIC_FILES || state == DYNAMIC_FILES_SET)) {
            args.dynamic_files.push_back(argv[i]);
            state = DYNAMIC_FILES_SET;
        }
        else if (!is_opt && state == JOB_ID) {
            args.job_id = argv[i];
            state = JOB_ID_SET;
        }
        else if (!is_opt && state == N_WORKERS) {
            args.n_workers_set = true;
            args.n_workers = str_to_unsigned(argv[i]);
            state = NONE;
        }
        else {
            print_help_and_exit(argv[0]);
        }

    }

    // check end states
    if (!(state == NONE ||
          state == STATIC_FILES_SET || state == DYNAMIC_FILES_SET ||
          state == JOB_ID_SET)) {
        print_help_and_exit(argv[0]);
    }

    if (args.work_job) {
        // require that job_id is valid
        if (!is_job_hash(args.job_id))
            print_help_and_exit(argv[0]);
    }
}

