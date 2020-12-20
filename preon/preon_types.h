#ifndef __preon_types_h__
#define __preon_types_h__

#include <string>
#include <vector>

struct PreonAddr {
    std::string     ip_addr;
    unsigned short  port;
};

inline bool operator==(const PreonAddr &a, const PreonAddr &b) {
    return a.ip_addr == b.ip_addr && a.port == b.port;
}

inline bool operator<(const PreonAddr &a, const PreonAddr &b) {
    if (a.ip_addr < b.ip_addr)
        return true;
    return a.port < b.port;
}

typedef std::vector<PreonAddr> PreonAddrList;

struct File {
    std::string name;
    std::string hash;
    uint64_t    size;
    bool        dynamic;
};

inline bool operator<(const File &a, const File &b) {
        return a.name < b.name;
}

#endif // #ifndef __preon_types_h__
