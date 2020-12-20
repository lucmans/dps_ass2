#include "program_state.h"
#include "error.h"

ProgramState::ProgramState() {
    m_config = nullptr;
}

void ProgramState::add_job(const std::string &root_dir, const std::string &job_id) {
    m_lock.lock();

    std::set<std::string> jobs;
    unsafe_get_job_ids(jobs);
    if (jobs.find(job_id) != jobs.end())
        return;

    std::string job_dir = root_dir + "/" + job_id;
    m_jobs.push_back(std::make_unique<Job>(job_dir, job_id));
    m_unfinished_job_ids.insert(job_id);

    m_lock.unlock();
}

Job *ProgramState::get_job(const std::string &job_id) const {
    Job *job = nullptr;

    m_lock.lock();
    for (const std::unique_ptr<Job> &j: m_jobs) {
        if (j->get_job_id() == job_id) {
            job = j.get();
            break;
        }
    }
    m_lock.unlock();

    return job;
}

void ProgramState::get_job_ids(std::set<std::string> &job_ids) {
    m_lock.lock();
    unsafe_get_job_ids(job_ids);
    m_lock.unlock();
}

std::string ProgramState::claim_unfinished_job_id() {
    std::string job_id;

    m_lock.lock();

    auto it = m_unfinished_job_ids.begin();
    if (it == m_unfinished_job_ids.end()) {
        m_lock.unlock();
        return "idle";
    }

    job_id = *it;

    m_unfinished_job_ids.erase(job_id);
    m_working_job_ids.insert(job_id);

    m_lock.unlock();

    return job_id;
}

void ProgramState::mark_finished_job_id(const std::string &job_id) {
    m_lock.lock();

    m_working_job_ids.erase(job_id);
    m_finished_job_ids.insert(job_id);

    m_lock.unlock();
}

unsigned ProgramState::get_n_idle_workers() {
    unsigned result;

    m_lock.lock();

    unsigned n_workers = m_config->get_n_workers();
    result = n_workers - m_working_job_ids.size();

    m_lock.unlock();

    return result;
}

void ProgramState::set_config(Config *config) {
    m_lock.lock();

    if (m_config != nullptr) {
        m_lock.unlock();
        throw PE("set_config() can only be called once");
    }
    m_config = config;

    m_lock.unlock();
}

Config *ProgramState::get_config() const {
    Config *result;

    m_lock.lock();
    result = m_config;
    m_lock.unlock();

    return result;
}

void ProgramState::unsafe_get_job_ids(std::set<std::string> &job_ids) {
    job_ids.clear();

    job_ids.insert(m_unfinished_job_ids.begin(), m_unfinished_job_ids.end());
    job_ids.insert(m_finished_job_ids.begin(), m_finished_job_ids.end());
    job_ids.insert(m_working_job_ids.begin(), m_working_job_ids.end());
}
