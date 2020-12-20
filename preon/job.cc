#include "consts.h"
#include "error.h"
#include "job.h"
#include "utils.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cctype>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>


Job::Job(const std::string &_dir, const std::string &_id) :
    id(_id), dir(_dir),
    manifest(_dir + "/" + PREON_MANIFEST_FILE),
    status(_dir + "/" + PREON_STATUS_FILE)
{ }

std::string Job::get_job_id() {
    return id;
}

std::string Job::get_job_dir() {
    return dir;
}

std::string Job::get_manifest() {
    std::string result;

    lock.lock();
    result = manifest.get_text();
    lock.unlock();

    return result;
}

void Job::read_manifest() {
    lock.lock();

    try {
        manifest.read();
    }
    catch (PreonExcept &e) {
        lock.unlock();
        throw;
    }

    std::vector<File> files;
    manifest.get_files(files);

    if (status.read(files)) {
        lock.unlock();
        return;
    }
    else {
        status.init(files);
        status.write();
    }

    if (status.get_execution_status())
        unsafe_hash_dynamic_files();

    lock.unlock();
}

bool Job::is_fishined(const std::string &filename) {
    bool result;

    lock.lock();
    result = unsafe_is_fishined(filename);
    lock.unlock();

    return result;
}

bool Job::is_master() {
    bool result;

    lock.lock();
    result = status.get_master();
    lock.unlock();

    return result;
}

void Job::write_block(const std::string &filename, int block_id, const std::vector<uint8_t> &data) {
    lock.lock();
    try {
        unsafe_write_block(filename, block_id, data);
    }
    catch (PreonExcept &e){
        lock.unlock();
        throw e;
    }
    lock.unlock();
}

bool Job::read_block(const std::string &filename, int block_id, std::vector<uint8_t> &data) {
    bool result;

    lock.lock();
    try {
        result = unsafe_read_block(filename, block_id, data);
    }
    catch (PreonExcept &e) {
        lock.unlock();
        throw e;
    }
    lock.unlock();

    return result;
}

int Job::claim_empty_block(const std::string &filename) {
    int result;

    lock.lock();
    result = status.file_claim_empty_block(filename);
    lock.unlock();

    return result;
}

void Job::get_files(std::vector<File> &files) {
    files.clear();

    lock.lock();

    if (manifest.get_text_empty()) {
        lock.unlock();
        throw PE("Manifest_text is empty");
    }

    manifest.get_files(files);

    lock.unlock();
}

void Job::reset_file(const std::string &filename) {
    lock.lock();

    status.reset_file(filename);
    status.write();

    lock.unlock();
}

void Job::update_dynamic_metadata(const std::vector<File> &dynamic_metadata) {
    lock.lock();

    if (!status.get_master()) {
        lock.unlock();
        throw PE("This function can only be called by a master");
    }

    for (const File &file: dynamic_metadata) {
        if (!file.dynamic) {
            lock.unlock();
            throw PE("Only dynamic files can be added");
        }

        manifest.add_file(file);
        status.set_file(file);
    }

    status.write();

    lock.unlock();
}

std::string Job::get_exec_cmd() {
    std::string result;

    lock.lock();
    result = manifest.get_exec_cmd();
    lock.unlock();

    return result;
}

void Job::execution_finished() {
    lock.lock();

    status.set_execution_status(true);

    if (!status.get_master()) {
        try {
            unsafe_hash_dynamic_files();
        }
        catch (PreonExcept &e) {
            error("Job: " + id + " failed to verify");
        }
    }

    status.write();

    lock.unlock();
}

void Job::hash_dynamic_files() {
    lock.lock();

    try {
        unsafe_hash_dynamic_files();
    }
    catch(PreonExcept &e) {
        error("Job: '" + id + "' Failed to hash files");
        lock.unlock();
        throw e;
    }

    lock.unlock();
}

bool Job::get_execution_finished() {
    bool result;

    lock.lock();
    result = status.get_execution_status();
    lock.unlock();

    return result;
}

bool Job::unsafe_is_fishined(const std::string &filename) {
    try {
        File file = manifest.get_file(filename);
        if (file.hash.empty()) {
            return false;
        }

        /*
        Blocks blocks = status.get_file(filename);
        return blocks.finished();
        */
        return status.file_is_finished(filename);
    }
    catch (PreonExcept &e) {
        warn(e.what());
    }

    return false;
}

void Job::unsafe_write_block(const std::string &filename, int block_id, const std::vector<uint8_t> &data) {
    int n_blocks = status.file_get_n_blocks(filename);
    if (block_id < 0 || block_id >= n_blocks)
        throw PE("Invalid block size");
    File file = manifest.get_file(filename);

    size_t pos = block_id * PREON_BLOCK_SIZE;
    size_t block_size = std::min(PREON_BLOCK_SIZE, file.size - pos);

    if (block_size != data.size())
        throw PE("Block had invalid size");

    std::string path = dir + "/" + filename;
    int fd = open(path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
        throw PE_SYS("open");

    if (lseek(fd, pos, SEEK_SET) == -1) {
        close(fd);
        throw PE_SYS("seek");
    }

    size_t bytes_written = 0;
    while (bytes_written < block_size) {
        ssize_t ret = write(fd, data.data() + bytes_written, block_size - bytes_written);
        if (ret == -1) {
            close(fd);
            throw PE_SYS("write");
        }

        bytes_written += (size_t)ret;
    }

    close(fd);

    status.set_block_status(filename, block_id, DONE);
    status.write();
}

bool Job::unsafe_read_block(const std::string &filename, int block_id, std::vector<uint8_t> &data) {
    if (!status.file_has_block(filename, block_id))
        return false;

    File file = manifest.get_file(filename);

    size_t pos = block_id * PREON_BLOCK_SIZE;
    size_t block_size = std::min(PREON_BLOCK_SIZE, file.size - pos);

    data.assign(block_size, 0);

    std::string path = dir + "/" + filename;
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1)
        throw PE_SYS("open");

    if (lseek(fd, pos, SEEK_SET) == -1) {
        close(fd);
        throw PE_SYS("seek");
    }

    size_t bytes_read = 0;
    while (bytes_read < block_size) {
        ssize_t ret = read(fd, data.data() + bytes_read, block_size - bytes_read);
        if (ret == -1) {
            close(fd);
            throw PE_SYS("read");
        }
        else if (ret == 0) {
            close(fd);
            throw PE_SYS("File to small");
        }

        bytes_read += (size_t)ret;
    }

    close(fd);

    return true;
}

void Job::unsafe_hash_dynamic_files() {
    std::vector<File> files;
    manifest.get_files(files);

    for (File &file: files) {
        if (!file.dynamic)
            continue;
        std::string filename = dir + "/" + file.name;

        file.hash = calc_hash(filename);

        struct stat stat_buf;
        if (lstat(filename.c_str(), &stat_buf) == -1)
            throw PE_SYS("lstat");
        file.size = (uint64_t)stat_buf.st_size;

        status.set_file(file, DONE);
        manifest.add_file(file);
    }
}

