#ifndef __error_h__
#define __error_h__

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <string>
#include <unistd.h>

inline std::string __ex_msg(const char *file, int line, const std::string &msg) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s:%d %s", file, line, msg.c_str());

    return buf;
}

inline void __err_msg(const char *type, const char *file, int line, const std::string &msg) {
    const char *start_color = "";
    const char *end_colour  = "\e[39m";
    if (strcmp(type, "error") == 0)
        start_color = "\e[31m";
    else if (strcmp(type, "warn") == 0)
        start_color = "\e[33m";
    else if (strcmp(type, "info") == 0)
        start_color = "\e[32m";
    else if (strcmp(type, "debug") == 0)
        start_color = "\e[34m";

    if (isatty(STDERR_FILENO) != 1) {
        start_color = "";
        end_colour = "";
    }

    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    double seconds = tp.tv_sec + (tp.tv_nsec / 1000000000.0);

    fprintf(stderr, "[%f] %s%s%s %s:%d %s\n",
            seconds, start_color, type, end_colour, file, line, msg.c_str());
}

class PreonExcept : public std::runtime_error {
    public:
        PreonExcept(const std::string &msg) : std::runtime_error(msg) {};
};

inline std::string __str(const char *x) {
    return std::string(x);
}

template <typename T>
std::string __str(T x) {
    return std::to_string(x);
}

#define STR(x)          __str(x)

#define ex_msg(msg)     __ex_msg(__FILE__, __LINE__, (msg))
#define PE(msg)         PreonExcept(ex_msg((msg)))
#define PE_SYS(sys)     PreonExcept(ex_msg(std::string((sys)) + "(): " + strerror(errno)))

#define error(msg)      __err_msg("error",  __FILE__, __LINE__, (msg))
#define warn(msg)       __err_msg("warn",   __FILE__, __LINE__, (msg))
#define info(msg)       __err_msg("info",   __FILE__, __LINE__, (msg))
#define debug(msg)      __err_msg("debug",  __FILE__, __LINE__, (msg))

#endif //#ifndef __error_h__
