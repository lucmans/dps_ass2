#include "job_worker.h"
#include "job.h"
#include "program_state.h"
#include "network_listener.h"
#include "network_client.h"
#include "config.h"
#include "error.h"
#include "consts.h"
#include "utils.h"
#include "preon_types.h"
#include "tracker.h"
#include "parse_args.h"

#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <thread>
#include <sstream>
#include <fcntl.h>
#include <fstream>
#include <dirent.h>
#include <libgen.h>
#include <algorithm>
#include <iterator>
#include <linux/limits.h>
#include <csignal>

namespace {

void except_create_job(const std::string &tmp_dir, Config &config,
        const std::vector<std::string> &static_files,
        const std::string &exec_cmd, const std::vector<std::string> &dynamic_files) {
    create_dir(tmp_dir, true);

    std::string manifest_file = tmp_dir + "/" + PREON_MANIFEST_FILE;
    Manifest manifest(manifest_file);

    std::string status_file = tmp_dir + "/" + PREON_STATUS_FILE;
    Status status(status_file);
    status.set_master(true);

    for (const std::string &file: static_files) {
        char buf[PATH_MAX + 1];
        strncpy(buf, file.c_str(), PATH_MAX);
        buf[PATH_MAX] = '\0';
        std::string file_basename = basename(buf);
        std::string file_path = tmp_dir + "/" + file_basename;

        copy_file(file_path, file);

        File f = {
            .name = file_basename,
            .hash = calc_hash(file_path),
            .size = file_size(file_path),
            .dynamic = false,
        };
        manifest.add_file(f);
        status.add_file(f);
    }

    if (exec_cmd != "")
        manifest.set_exec_cmd(exec_cmd);

    // if exec_cmd == "", this will be empty
    for (const std::string &file: dynamic_files) {
        char buf[PATH_MAX + 1];
        strncpy(buf, file.c_str(), PATH_MAX);
        buf[PATH_MAX] = '\0';
        std::string file_basename = basename(buf);

        File f = {
            .name = file_basename,
            .hash = "",
            .size = 0,
            .dynamic = true,
        };
        manifest.add_file(f);
    }

    manifest.write();
    status.write();

    std::string job_id = calc_hash(manifest_file);
    std::string new_job_folder = config.get_download_folder() + "/" + job_id;
    if (rename(tmp_dir.c_str(), new_job_folder.c_str()) == -1) {
        throw PE_SYS("rename");
    }

    info("Created new job '" + job_id + "'");
}

void create_job(Config &config, const std::vector<std::string> &static_files,
        const std::string &exec_cmd, const std::vector<std::string> &dynamic_files) {
    std::string tmp_dir = config.get_download_folder() + "/" + random_string(10);
    try {
        except_create_job(tmp_dir, config, static_files, exec_cmd, dynamic_files);
    }
    catch (PreonExcept &e) {
        error(STR("Failed to create job: ") + e.what());
        remove_dir(tmp_dir);
        exit(EXIT_FAILURE);
    }
}

void work_job(Config &config, const std::string &job_id) {
    std::string job_folder = config.get_download_folder() + "/" + job_id;
    try {
        create_dir(job_folder, true);
    }
    catch (PreonExcept &e) {
        error(STR("Adding job failed: ") + e.what());
        exit(EXIT_FAILURE);
    }

    info("job '" + job_id + "' created");
}

void scan_jobs(std::set<std::string> &job_ids, const std::string &job_root) {
    job_ids.clear();

    create_dir(job_root);
    DIR *dir = opendir(job_root.c_str());
    if (dir == nullptr)
        throw PE_SYS("opendir");

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string name = ent->d_name;
        if (!is_job_hash(name))
            continue;
        else if (!(ent->d_type & DT_DIR))
            continue;

        job_ids.insert(ent->d_name);
    }

    closedir(dir);
}

void create_lock() {
    mode_t mode = S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int fd = open(PREON_LOCK_FILE.c_str(),  O_CREAT | O_EXCL, mode);
    if (fd == -1 && errno == EEXIST) {
        info("Preon already running. (If this is not the case remove '" + PREON_LOCK_FILE + "'");
        exit(EXIT_SUCCESS);
    }
    else if (fd == -1) {
        throw PE_SYS("open");
    }
    close(fd);
}

void remove_lock() {
    remove(PREON_LOCK_FILE.c_str());
}

void sig_handler(int signo) {
    (void)signo;
    exit(EXIT_SUCCESS);
}

void conn_worker_thread(ProgramState &state, int fd) {
    NetworkClient client(fd, &state);

    // handle incoming connections
    client.wait();
}

void worker_thread(ProgramState &state) {
    // random delay to prevent a lot of workers
    // from starting at the same time
    usleep(RANDOM_DELAY * ((double)rand() / RAND_MAX));

    Config *config = state.get_config();
    Tracker t(config->get_tracker_url(), config->get_tracker_port());

    time_t next_idle_report = 0;
    for (;;) {
        std::string job_id = state.claim_unfinished_job_id();
        if (job_id == "idle") {
            if (next_idle_report < time(nullptr)) {
                t.inform_job(config->get_listen_port(), "idle"); // TODO this will cause a lot of messages to the tracker. Better save the next_idle in program state
                next_idle_report = time(nullptr) + IDLE_REPORT_TIME;
            }
            usleep(WORKER_THREAD_TIMEOUT);
        }
        else {
            if (state.get_n_idle_workers() == 0)
                t.remove_job(config->get_listen_port(), "idle");
            JobWorker jw(job_id, state);
            jw.work();
            state.mark_finished_job_id(job_id);
        }
    }
}

void fs_watch_thread(ProgramState &state) {
    Config *config = state.get_config();
    Tracker tracker(config->get_tracker_url(), config->get_tracker_port());

    std::string download_folder = config->get_download_folder();
    for (;;) {
        std::set<std::string> job_ids_fs;
        scan_jobs(job_ids_fs, download_folder);

        std::set<std::string> job_ids;
        state.get_job_ids(job_ids);

        std::set<std::string> job_ids_new;
        std::set_difference(job_ids_fs.begin(), job_ids_fs.end(),
                job_ids.begin(), job_ids.end(),
                std::inserter(job_ids_new, job_ids_new.begin()));

        for (const std::string &job_id: job_ids_new) {
            info("fs_watch: new job: '" + job_id + "'");

            state.add_job(download_folder, job_id);
            tracker.inform_job(config->get_listen_port(), job_id);
        }

        usleep(FS_WATCH_TIMEOUT);
    }
}

}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(nullptr));

    char buf[PATH_MAX + 1];
    strncpy(buf, argv[0], PATH_MAX);
    buf[PATH_MAX] = '\0';
    std::string config_filename = std::string(dirname(buf)) + "/" + PREON_CONFIG_FILE;
    Config config(config_filename);

    Args args;
    parse_args(argc, argv, args);
    if (args.create_job)
        create_job(config, args.static_files, args.exec_cmd, args.dynamic_files);
    else if (args.work_job)
        work_job(config, args.job_id);

    if (args.n_workers_set)
        config.set_n_workers(args.n_workers);

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    create_lock();
    atexit(remove_lock);

    ProgramState state;
    state.set_config(&config);

    Tracker tracker(config.get_tracker_url(), config.get_tracker_port());

    // Scan filesystem for all jobs and make datastructure to track job/download progress
    std::set<std::string> job_ids;
    scan_jobs(job_ids, config.get_download_folder());
    for (const std::string &job_id: job_ids) {
        state.add_job(config.get_download_folder(), job_id);
        tracker.inform_job(config.get_listen_port(), job_id);
    }

    // start the worker threads
    unsigned n_workers = config.get_n_workers();
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < n_workers; i++)
        threads.push_back(std::thread(worker_thread, std::ref(state)));

    // start a fs watcher to detect new jobs being added locally
    threads.push_back(std::thread(fs_watch_thread, std::ref(state)));

    // network listener
    NetworkListener listener(config.get_listen_port());
    for (;;) {
        int fd = listener.wait();
        threads.push_back(std::thread(conn_worker_thread, std::ref(state), fd));
    }

    // wait for all threads to complete
    for (auto &t: threads)
        t.join();

    return EXIT_SUCCESS;
}

