#ifndef SLIC3R_FileSystem_LOG_HPP_
#define SLIC3R_FileSystem_LOG_HPP_
#include <string>

namespace Slic3r {

extern void set_logging_level(unsigned int level);
extern unsigned int level_string_to_boost(std::string level);
extern std::string  get_string_logging_level(unsigned level);
extern unsigned get_logging_level();
extern void trace(unsigned int level, const char *message);

// smaller level means less log. level=5 means saving all logs.
void set_log_path_and_level(const std::string& file, unsigned int level);
void flush_logs();

// Format memory allocated, separate thousands by comma.
extern std::string format_memsize_MB(size_t n);
// Return string to be added to the boost::log output to inform about the current process memory allocation.
// The string is non-empty if the loglevel >= info (3) or ignore_loglevel==true.
// Latter is used to get the memory info from SysInfoDialog.
extern std::string log_memory_info(bool ignore_loglevel = false);

// Returns the size of physical memory (RAM) in bytes.
extern size_t total_physical_memory();
}

#endif // SLIC3R_FileSystem_LOG_HPP_