#ifndef __consts_h__
#define __consts_h__

#include <cstddef>
#include <string>

const std::string   PREON_LOCK_FILE     = "/tmp/preon.lock";
const std::string   PREON_STATUS_FILE   = "status.txt";
const std::string   PREON_MANIFEST_FILE = "manifest.txt";
const std::string   PREON_CONFIG_FILE   = "preon.conf";
const size_t        PREON_BLOCK_SIZE    = 1024 * 1024; // 1 MiB

const int WORKER_THREAD_TIMEOUT         = 1 * 1000000;  // s * μs/s
const int FS_WATCH_TIMEOUT              = 1 * 1000000;  // s * μs/s
const int RETRY_TIME                    = 60 * 1000000; // s * μs/s
const int RANDOM_DELAY                  = 10 * 1000000; // 10 s

const int IDLE_REPORT_TIME              = 60;           // s

#endif //#ifndef __consts_h__
