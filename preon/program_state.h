#ifndef __program_state_h__
#define __program_state_h__

#include "job.h"
#include "config.h"

#include <set>
#include <string>
#include <mutex>
#include <memory>

class ProgramState {
    public:
        ProgramState();

        void add_job(const std::string &root_dir, const std::string &job_id);
        Job *get_job(const std::string &job_id) const;

        void get_job_ids(std::set<std::string> &job_ids);

        std::string claim_unfinished_job_id();
        void mark_finished_job_id(const std::string &job_id);

        unsigned get_n_idle_workers();

        void set_config(Config *config);
        Config *get_config() const;

    private:
        mutable std::mutex                  m_lock;
        std::vector<std::unique_ptr<Job>>   m_jobs;
        Config                             *m_config;

        std::set<std::string>               m_unfinished_job_ids;
        std::set<std::string>               m_finished_job_ids;
        std::set<std::string>               m_working_job_ids;

        void unsafe_get_job_ids(std::set<std::string> &job_ids);
};

#endif //#ifndef __program_state_h__
