#include "utils.h"
#include "error.h"
#include "sha256.h"

#include <limits>
#include <string>
#include <cctype>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <dirent.h>

std::string lstrip(const std::string &str) {
    size_t begin = 0;
    while (begin < str.size() && isspace(str[begin]))
        begin++;

    return std::string(str.begin() + begin, str.end());
}

std::string rstrip(const std::string &str) {
    size_t end = str.size();
    while (end > 0 && isspace(str[end - 1]))
        end--;
    return std::string(str.begin(), str.begin() + end);
}

std::string strip(const std::string &str) {
    return rstrip(lstrip(str));
}

void split(const std::string s, std::vector<std::string> &tokens, const char delim) {
    std::stringstream ss(s);
    std::string token;

    tokens.clear();
    while(std::getline(ss, token, delim))
        tokens.push_back(std::move(token));
}

std::string random_string(size_t size) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1)
        throw PE_SYS("open");

    std::string result;
    for (size_t i = 0; i < size; i++) {
        uint8_t c;
        if (read(fd, &c, 1) != 1) {
            close(fd);
            throw PE_SYS("read");
        }
        result += 'a' + (c % 26);
    }

    close(fd);

    return result;
}

bool is_ip(const std::string ip) {
    if (ip.compare("localhost") == 0)
        return true;

    std::vector<std::string> fields;
    split(ip, fields, '.');

    if (fields.size() != 4)
        return false;

    for (std::string &field : fields) {
        try {
            int f = std::stoi(field);
            if (f < 0 || f > 255)
                return false;
        }
        catch (...) {
            return false;
        }
    }

    return true;
}

bool is_port(const std::string port) {
    try {
        int p = std::stoi(port);
        if (p < 0 || p > 65535)
            return false;
    }
    catch (...) {
        return false;
    }

    return true;
}

bool is_job_hash(const std::string &job_id) {
    if (job_id.size() != 64)
        return false;

    for (int i = 0; i < 64; i++) {
        char c = job_id[i];
        if ((c < '0' || c > '9') && (c < 'a' || c > 'f'))
            return false;
    }

    return true;
}

unsigned str_to_unsigned(const std::string &str) {
    for (char c: str) {
        if (!isdigit(c))
            throw PE("Invalid unsigned integer");
    }

    unsigned long x;
    try {
        x = std::stoul(str);
    }
    catch (...) {
        throw PE("Invalid unsigned integer");
    }

    if (x > std::numeric_limits<unsigned>::max())
        throw PE("Invalid unsigned integer");

    return static_cast<unsigned>(x);
}

unsigned short str_to_port(const std::string &value) {
    unsigned x;
    try {
        x = str_to_unsigned(value);
    }
    catch (PreonExcept &e) {
        throw PE("Invalid port number");
    }

    if (x > 65535)
        throw std::runtime_error("Invalid port number");

    return static_cast<unsigned short>(x);
}

std::string calc_hash(const std::string &filename) {
    SHA256_CTX ctx;
    sha256_init(&ctx);

    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
        throw PE_SYS("open");

    for (;;) {
        char buf[1024];
        ssize_t ret = read(fd, buf, sizeof(buf));
        if (ret == -1) {
            close(fd);
            throw PE_SYS("read");
        }
        else if (ret == 0)
            break;

        sha256_update(&ctx, (const BYTE *)buf, (size_t)ret);
    }

    close(fd);

    BYTE hash[SHA256_BLOCK_SIZE];
    sha256_final(&ctx, hash);

    return sha256_to_str(hash);
}

size_t file_size(const std::string &filename) {
    struct stat statbuf;
    if (lstat(filename.c_str(), &statbuf) == -1)
        throw PE_SYS("lstat");

    return statbuf.st_size;
}

void create_dir(std::string dirname, bool fail_if_exists) {
    while (dirname != "/" && dirname.back() == '/')
        dirname.pop_back();

    size_t sep = 0;
    while (sep != dirname.size()) {
        sep = dirname.find('/', sep);
        if (sep == std::string::npos)
            sep = dirname.size();
        else
            sep++;
        std::string dir(dirname.begin(), dirname.begin() + sep);

        int ret = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if (ret == -1 && errno != EEXIST)
            warn(std::string("mkdir: ") + strerror(errno));

        // if we cannot create the final dir error
        if (sep == dirname.size() && ret == -1) {
            if (fail_if_exists || errno != EEXIST)
                throw PE_SYS("mkdir");
        }
    }

    struct stat stat_buf;
    int ret = lstat(dirname.c_str(), &stat_buf);
    if (ret == -1)
        throw PE_SYS("lstat");
}

void remove_dir(const std::string &dirname) {
    DIR *dir = opendir(dirname.c_str());
    if (dir == nullptr)
        throw PE_SYS("opendir");

    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        std::string child = dirname + "/" + ent->d_name;
        if (remove(child.c_str()) == -1) {
            closedir(dir);
            throw PE_SYS("remove");
        }
    }
    closedir(dir);

    if (remove(dirname.c_str()) == -1)
        throw PE_SYS("remove");
}

void copy_file(const std::string &dst, const std::string &src) {
    int source = open(src.c_str(), O_RDONLY, 0);
    if (source == -1)
        throw PE_SYS("open");

    int dest = open(dst.c_str(), O_WRONLY | O_CREAT, 0644);
    if (dest == -1) {
        close(source);
        throw PE_SYS("open");
    }

    struct stat stat_source;
    if (fstat(source, &stat_source) == -1) {
        close(source);
        close(dest);
        throw PE_SYS("fstat");
    }

    if (sendfile(dest, source, 0, stat_source.st_size) == -1) {
        close(source);
        close(dest);
        throw PE_SYS("sendfile");
    }

    close(source);
    close(dest);
}

std::string file_to_str(const std::string &filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
        throw PE_SYS("open");

    std::string output;
    for (;;) {
        char buf[1024];
        ssize_t ret = read(fd, buf, sizeof(buf));
        if (ret == 0)
            break;
        else if (ret == -1) {
            close(fd);
            throw PE_SYS("read");
        }

        for (ssize_t i = 0; i < ret; i++) {
            if (buf[i] == 0) {
                close(fd);
                throw PE("file_to_str file cannot contain \\0 character");
            }
            output += buf[i];
        }
    }

    close(fd);

    return output;
}

void write_file(const std::string &filename, const std::string &src) {
    std::ofstream file(filename);

    if (!file.is_open())
        throw PE("Failed to open file");

    file.write(src.c_str(), src.size());

    if (!file.good())
        throw PE("Writing failed");

    file.close();
}
