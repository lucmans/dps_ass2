#ifndef __utils_h__
#define __utils_h__

#include <string>
#include <vector>

// String manipulation
std::string lstrip(const std::string &str);
std::string rstrip(const std::string &str);
std::string strip(const std::string &str);
void split(const std::string s, std::vector<std::string> &tokens, const char delim);
std::string random_string(size_t size);

bool is_ip(const std::string ip);
bool is_port(const std::string port);
bool is_job_hash(const std::string &job_id);

unsigned str_to_unsigned(const std::string &str);
unsigned short str_to_port(const std::string &value);

std::string calc_hash(const std::string &filename);
size_t file_size(const std::string &filename);
void create_dir(std::string dirname, bool fail_if_exists = false);
void remove_dir(const std::string &dirname);
void copy_file(const std::string &dst, const std::string &src);

std::string file_to_str(const std::string &filename);
void write_file(const std::string &filename, const std::string &src);

#endif //#ifndef __utils_h__
