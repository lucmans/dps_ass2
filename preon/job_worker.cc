#include "error.h"
#include "network_client.h"
#include "job_worker.h"
#include "consts.h"
#include "sha256.h"
#include "utils.h"
#include "tracker.h"

#include <climits>
#include <fstream>
#include <set>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>


JobWorker::JobWorker(const std::string &_job_id, const ProgramState &state) {
    job_id = _job_id;

    config = state.get_config();
    job = state.get_job(job_id);
}

void JobWorker::work() {
    info("job worker starting for: " + job_id);

    Tracker t(config->get_tracker_url(), config->get_tracker_port());

    // 1) check if manifest exists
    std::string manifest_filename = job->get_job_dir() + "/" + PREON_MANIFEST_FILE;
    struct stat stat_buf;
    int ret = lstat(manifest_filename.c_str(), &stat_buf);
    if (ret == -1 && errno == ENOENT) {
        std::string manifest;
        download_manifest(manifest, t);
        write_file(manifest_filename, manifest);
    }
    else if (ret == -1)
        throw std::runtime_error(strerror(errno));
    job->read_manifest();

    // 1.25) if master, inform a idle worker that we have a job for it
    if (job->is_master() && !job->get_execution_finished())
        inform_idle_worker();

    // 1.5) make the master wait for the dynamic_file meta data
    if (job->is_master() && !job->get_execution_finished()) {
        download_dynamic_files_metadata();
    }

    // 2) Download files and verify
    do {
        download_files();
    } while (!verify_files());

    // 3) preform execution job (if we are a worker and we have not
    // finished the computation)
    if (!job->is_master() && !job->get_execution_finished())
        execute_job();

    // 4) done
    job->execution_finished();
    info("job worker finished: " + job_id);
}

void JobWorker::download_manifest(std::string &manifest, Tracker &t) {
    manifest.clear();

    while (manifest.empty()) {
        PreonAddrList list = t.query_job(job_id);

        for (const PreonAddr &addr: list) {
            try {
                NetworkClient conn(addr, NULL);
                if (conn.get_manifest(job_id, manifest))
                    return; // successfully obtained a manifest file
            }
            catch (PreonExcept &e) {
                debug(e.what());
                continue;
            }
        }

        info("Attempt to fetch manifest for: '" + job_id + "' failed. Waiting "
                + STR(RETRY_TIME / 1000000) + " seconds");
        usleep(RETRY_TIME);
    }
}

void JobWorker::download_file(const File &file) {
    Tracker t(config->get_tracker_url(), config->get_tracker_port());

    int block_id;
    while ((block_id = job->claim_empty_block(file.name)) != -1) {
        for (;;) {
            PreonAddrList list = t.query_job(job_id);
            while (!list.empty()) {
                size_t pos = rand() % list.size();
                PreonAddr addr = list[pos];
                list.erase(list.begin() + pos);

                try {
                    NetworkClient client(addr, nullptr);
                    std::vector<uint8_t> block;
                    block.reserve(PREON_BLOCK_SIZE);
                    if (client.get_block(job_id, file.name, block_id, block)) {
                        job->write_block(file.name, block_id, block);
                        goto block_done;
                    }
                }
                catch(PreonExcept &e) {
                    debug(e.what());
                }
            }

            info("block: " + job_id + ":" + file.name + ":" + STR(block_id) +
                    " not available. Retry in " + STR(RETRY_TIME / 1000000) + "s");
            usleep(RETRY_TIME);
        }
block_done:
        ;
    }
}

void JobWorker::download_files() {
    std::vector<File> files;
    job->get_files(files);
    for (const File &file: files) {
        if (file.dynamic && !job->is_master())
            continue;

        if (job->is_fishined(file.name))
            continue;

        download_file(file);
        info("Done downloading: '" + job->get_job_dir() + "/" + file.name + "'");
    }
}

bool JobWorker::verify_files() {
    std::string dir = job->get_job_dir();

    bool success = true;
    std::vector<File> files;
    job->get_files(files);
    for (const File &f: files) {
        std::string filename = dir + "/" + f.name;

        // this only happens for dynamic files
        if (f.hash.empty()) {
            if (job->is_master())
                warn("Skipping check for '" + filename + "' because meta data is missing");
            continue;
        }

        if (calc_hash(filename) != f.hash) {
            success = false;
            warn(filename + " failed to verify");
            job->reset_file(f.name);
        }
    }

    if (success)
        info("files for job: " + job_id + " verified successfully");

    return success;
}

void JobWorker::execute_job() {
    std::string exec_cmd = job->get_exec_cmd();
    std::string working_dir = job->get_job_dir();
    std::string pathname_str = working_dir + "/" + exec_cmd;

    if (pathname_str.size() - 1>= PATH_MAX)
        throw PE("pathname too long");

    mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH;
    if (chmod(pathname_str.c_str(), mode) == -1)
        throw PE_SYS("chmod");

    info("Executing " + job_id + " " + exec_cmd);

    char pathname[PATH_MAX];
    strcpy(pathname, pathname_str.c_str());

    char buf[PATH_MAX];
    strcpy(buf, basename(pathname_str.c_str()));

    char *argv[2] = { buf, NULL };

    pid_t pid = fork();
    if (pid == -1)
        throw PE_SYS("fork");

    if (pid == 0) {
        if (chdir(working_dir.c_str()) == -1)
            throw PE_SYS("chdir");
        if (execv(pathname, argv) == -1)
            throw PE_SYS("execv");
    }
    else {
        int status;
        if (waitpid(pid, &status, 0) == (pid_t)-1)
            throw PE_SYS("waitpid");
    }
}

void JobWorker::download_dynamic_files_metadata() {
    Tracker t(config->get_tracker_url(), config->get_tracker_port());

    for (;;) {
        PreonAddrList addr_list = t.query_job(job_id);
        for (const PreonAddr &addr: addr_list) {
            try {
                NetworkClient conn(addr, nullptr);

                std::vector<File> dynamic_files;
                if (conn.get_dynamic_metadata(job_id, dynamic_files)) {
                    job->update_dynamic_metadata(dynamic_files);
                    return;
                }
            }
            catch (PreonExcept &e) {
                debug(STR("while downloading meta data: ") + e.what());
            }
        }

        info("Dynamic meta data not available, trying again in " + STR(RETRY_TIME / 1000000) + "s");
        usleep(RETRY_TIME);
    }
}

void JobWorker::inform_idle_worker() {
    Tracker tracker(config->get_tracker_url(), config->get_tracker_port());

    for (;;) {
        PreonAddrList addr_list = tracker.query_job("idle");

        for (const PreonAddr &addr: addr_list) {
            try {
                NetworkClient client(addr, nullptr);

                if (client.inform_job(job_id))
                    return;
            }
            catch (PreonExcept &e) {
                debug(e.what());
            }
        }

        info("No worker available, retry in " + STR(RETRY_TIME / 1000000) + "s");
        usleep(RETRY_TIME);
    }
}

