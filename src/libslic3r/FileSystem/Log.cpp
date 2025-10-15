#include "Log.hpp"

#include "DataDir.hpp"

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/support/date_time.hpp>

#ifdef WIN32
	#include <windows.h>
    #include <psapi.h>
#else
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/param.h>
    #include <sys/resource.h>
	#ifdef BSD
		#include <sys/sysctl.h>
	#endif
	#ifdef __APPLE__
		#include <mach/mach.h>
		#include <libproc.h>
	#endif
	#ifdef __linux__
		#include <sys/stat.h>
		#include <fcntl.h>
		#include <sys/sendfile.h>
		#include <dirent.h>
		#include <stdio.h>
	#endif
#endif

namespace Slic3r {
static boost::log::trivial::severity_level logSeverity = boost::log::trivial::error;

static boost::log::trivial::severity_level level_to_boost(unsigned level)
{
    switch (level) {
    // Report fatal errors only.
    case 0: return boost::log::trivial::fatal;
    // Report fatal errors and errors.
    case 1: return boost::log::trivial::error;
    // Report fatal errors, errors and warnings.
    case 2: return boost::log::trivial::warning;
    // Report all errors, warnings and infos.
    case 3: return boost::log::trivial::info;
    // Report all errors, warnings, infos and debugging.
    case 4: return boost::log::trivial::debug;
    // Report everyting including fine level tracing information.
    default: return boost::log::trivial::trace;
    }
}

void set_logging_level(unsigned int level)
{
    logSeverity = level_to_boost(level);

    boost::log::core::get()->set_filter
    (
        boost::log::trivial::severity >= logSeverity
    );
}

unsigned int level_string_to_boost(std::string level)
{
    std::map<std::string, int> Control_Param;
    Control_Param["fatal"] = 0;
    Control_Param["error"] = 1;
    Control_Param["warning"] = 2;
    Control_Param["info"] = 3;
    Control_Param["debug"] = 4;
    Control_Param["trace"] = 5;

    return Control_Param[level];
}

std::string get_string_logging_level(unsigned level)
{
    switch (level) {
    case 0: return "fatal";
    case 1: return "error";
    case 2: return "warning";
    case 3: return "info";
    case 4: return "debug";
    case 5: return "trace";
    default: return "error";
    }
}

unsigned get_logging_level()
{
    switch (logSeverity) {
    case boost::log::trivial::fatal : return 0;
    case boost::log::trivial::error : return 1;
    case boost::log::trivial::warning : return 2;
    case boost::log::trivial::info : return 3;
    case boost::log::trivial::debug : return 4;
    case boost::log::trivial::trace : return 5;
    default: return 1;
    }
}

boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>> g_log_sink;
boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>> g_console_sink;

// Force set_logging_level(<=error) after loading of the DLL.
// This is currently only needed if libslic3r is loaded as a shared library into Perl interpreter
// to perform unit and integration tests.
static struct RunOnInit {
    RunOnInit() {
        set_logging_level(2);

    }
} g_RunOnInit;

void trace(unsigned int level, const char *message)
{
    boost::log::trivial::severity_level severity = level_to_boost(level);

    BOOST_LOG_STREAM_WITH_PARAMS(::boost::log::trivial::logger::get(),\
        (::boost::log::keywords::severity = severity)) << message;
}

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;
namespace attrs = boost::log::attributes;
void set_log_path_and_level(const std::string& file, unsigned int level)
{
#ifdef __APPLE__
	//currently on old macos, the boost::log::add_file_log will crash
	//TODO: need to be fixed
	if (!is_macos_support_boost_add_file_log()) {
		return;
	}
#endif

	//BBS log file at C:\\Users\\[yourname]\\AppData\\Roaming\\OrcaSlicer\\log\\[log_filename].log
	auto log_folder = boost::filesystem::path(user_data_dir()) / "log";
	if (!boost::filesystem::exists(log_folder)) {
		boost::filesystem::create_directory(log_folder);
	}
	auto full_path = (log_folder / file).make_preferred();

    if(file.empty()) {
        // add console log sink
        g_console_sink = boost::log::add_console_log(
            std::cout,
            keywords::format =
            (
                expr::stream
                << "[" << expr::attr< logging::trivial::severity_level >("Severity") << "]\t"
                << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                <<"[Thread " << expr::attr<attrs::current_thread_id::value_type>("ThreadID") << "]"
                << ":" << expr::smessage
            )
        );
    }else{
        // add fild log sink
        g_log_sink = boost::log::add_file_log(
            keywords::file_name = full_path.string() + ".%N",
            keywords::rotation_size = 100 * 1024 * 1024,
            keywords::format =
            (
                expr::stream
                << "[" << expr::attr< logging::trivial::severity_level >("Severity") << "]\t"
                << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                <<"[Thread " << expr::attr<attrs::current_thread_id::value_type>("ThreadID") << "]"
                << ":" << expr::smessage
            )
        );
    }

	logging::add_common_attributes();

	set_logging_level(level);

	return;
}

void flush_logs()
{
	if (g_log_sink)
		g_log_sink->flush();
    if (g_console_sink)
        g_console_sink->flush();

	return;
}

std::string format_memsize_MB(size_t n)
{
    std::string out;
    size_t n2 = 0;
    size_t scale = 1;
    // Round to MB
    n +=  500000;
    n /= 1000000;
    while (n >= 1000) {
        n2 = n2 + scale * (n % 1000);
        n /= 1000;
        scale *= 1000;
    }
    char buf[8];
    sprintf(buf, "%d", (int)n);
    out = buf;
    while (scale != 1) {
        scale /= 1000;
        n = n2 / scale;
        n2 = n2  % scale;
        sprintf(buf, ",%03d", (int)n);
        out += buf;
    }
    return out + "MB";
}

// Returns platform-specific string to be used as log output or parsed in SysInfoDialog.
// The latter parses the string with (semi)colons as separators, it should look about as
// "desc1: value1; desc2: value2" or similar (spaces should not matter).
std::string log_memory_info(bool ignore_loglevel)
{
    std::string out;
    if (ignore_loglevel || logSeverity <= boost::log::trivial::info) {
#ifdef WIN32
    #ifndef PROCESS_MEMORY_COUNTERS_EX
        // MingW32 doesn't have this struct in psapi.h
        typedef struct _PROCESS_MEMORY_COUNTERS_EX {
          DWORD  cb;
          DWORD  PageFaultCount;
          SIZE_T PeakWorkingSetSize;
          SIZE_T WorkingSetSize;
          SIZE_T QuotaPeakPagedPoolUsage;
          SIZE_T QuotaPagedPoolUsage;
          SIZE_T QuotaPeakNonPagedPoolUsage;
          SIZE_T QuotaNonPagedPoolUsage;
          SIZE_T PagefileUsage;
          SIZE_T PeakPagefileUsage;
          SIZE_T PrivateUsage;
        } PROCESS_MEMORY_COUNTERS_EX, *PPROCESS_MEMORY_COUNTERS_EX;
    #endif /* PROCESS_MEMORY_COUNTERS_EX */

        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
            out = " WorkingSet: " + format_memsize_MB(pmc.WorkingSetSize) + "; PrivateBytes: " + format_memsize_MB(pmc.PrivateUsage) + "; Pagefile(peak): " + format_memsize_MB(pmc.PagefileUsage) + "(" + format_memsize_MB(pmc.PeakPagefileUsage) + ")";
        else
            out += " Used memory: N/A";
#elif defined(__linux__) or defined(__APPLE__)
        // Get current memory usage.
    #ifdef __APPLE__
        struct mach_task_basic_info info;
        mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
        out += " Resident memory: ";
        if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount ) == KERN_SUCCESS )
            out += format_memsize_MB((size_t)info.resident_size);
        else
            out += "N/A";
    #else // i.e. __linux__
        size_t tSize = 0, resident = 0, share = 0;
        std::ifstream buffer("/proc/self/statm");
        if (buffer && (buffer >> tSize >> resident >> share)) {
            size_t page_size = (size_t)sysconf(_SC_PAGE_SIZE); // in case x86-64 is configured to use 2MB pages
            size_t rss = resident * page_size;
            out += " Resident memory: " + format_memsize_MB(rss);
            out += "; Shared memory: " + format_memsize_MB(share * page_size);
            out += "; Private memory: " + format_memsize_MB(rss - share * page_size);
        }
        else
            out += " Used memory: N/A";
    #endif
        // Now get peak memory usage.
        out += "; Peak memory usage: ";
        rusage memory_info;
        if (getrusage(RUSAGE_SELF, &memory_info) == 0)
        {
            size_t peak_mem_usage = (size_t)memory_info.ru_maxrss;
            #ifdef __linux__
                peak_mem_usage *= 1024;// getrusage returns the value in kB on linux
            #endif
            out += format_memsize_MB(peak_mem_usage);
        }
        else
            out += "N/A";
#endif
    }
    return out;
}

// Returns the size of physical memory (RAM) in bytes.
// http://nadeausoftware.com/articles/2012/09/c_c_tip_how_get_physical_memory_size_system
size_t total_physical_memory()
{
#if defined(_WIN32) && (defined(__CYGWIN__) || defined(__CYGWIN32__))
	// Cygwin under Windows. ------------------------------------
	// New 64-bit MEMORYSTATUSEX isn't available.  Use old 32.bit
	MEMORYSTATUS status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatus( &status );
	return (size_t)status.dwTotalPhys;
#elif defined(_WIN32)
	// Windows. -------------------------------------------------
	// Use new 64-bit MEMORYSTATUSEX, not old 32-bit MEMORYSTATUS
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx( &status );
	return (size_t)status.ullTotalPhys;
#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	// UNIX variants. -------------------------------------------
	// Prefer sysctl() over sysconf() except sysctl() HW_REALMEM and HW_PHYSMEM

#if defined(CTL_HW) && (defined(HW_MEMSIZE) || defined(HW_PHYSMEM64))
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_MEMSIZE)
	mib[1] = HW_MEMSIZE;            // OSX. ---------------------
#elif defined(HW_PHYSMEM64)
	mib[1] = HW_PHYSMEM64;          // NetBSD, OpenBSD. ---------
#endif
	int64_t size = 0;               // 64-bit
	size_t len = sizeof( size );
	if ( sysctl( mib, 2, &size, &len, NULL, 0 ) == 0 )
		return (size_t)size;
	return 0L;			// Failed?

#elif defined(_SC_AIX_REALMEM)
	// AIX. -----------------------------------------------------
	return (size_t)sysconf( _SC_AIX_REALMEM ) * (size_t)1024L;

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
	// FreeBSD, Linux, OpenBSD, and Solaris. --------------------
	return (size_t)sysconf( _SC_PHYS_PAGES ) *
		(size_t)sysconf( _SC_PAGESIZE );

#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGE_SIZE)
	// Legacy. --------------------------------------------------
	return (size_t)sysconf( _SC_PHYS_PAGES ) *
		(size_t)sysconf( _SC_PAGE_SIZE );

#elif defined(CTL_HW) && (defined(HW_PHYSMEM) || defined(HW_REALMEM))
	// DragonFly BSD, FreeBSD, NetBSD, OpenBSD, and OSX. --------
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_REALMEM)
	mib[1] = HW_REALMEM;		// FreeBSD. -----------------
#elif defined(HW_PYSMEM)
	mib[1] = HW_PHYSMEM;		// Others. ------------------
#endif
	unsigned int size = 0;		// 32-bit
	size_t len = sizeof( size );
	if ( sysctl( mib, 2, &size, &len, NULL, 0 ) == 0 )
		return (size_t)size;
	return 0L;			// Failed?
#endif // sysctl and sysconf variants

#else
	return 0L;			// Unknown OS.
#endif
}

}