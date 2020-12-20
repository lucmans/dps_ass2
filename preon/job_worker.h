#ifndef __job_worker_h__
#define __job_worker_h__

#include "program_state.h"
#include "tracker.h"

#include <string>

class JobWorker {
    public:
        JobWorker(const std::string &_job_id, const ProgramState &state);

        void work();

    private:
        void download_manifest(std::string &manifest, Tracker &t);
        void download_file(const File &file);
        void download_files();
        bool verify_files();
        void execute_job();
        void download_dynamic_files_metadata();
        void inform_idle_worker();

        std::string job_id;
        Config *config;
        Job *job;
};

#endif //ifndef __job_worker_h__
