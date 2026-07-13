#include "system-info.h"
#include "platform.h"

#include <fstream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#else
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#if !defined(__APPLE__)
static std::string trimValue(std::string value) {
    const size_t begin = value.find_first_not_of(" \t\r\n");
    const size_t end = value.find_last_not_of(" \t\r\n");
    return begin == std::string::npos ? std::string() : value.substr(begin, end - begin + 1);
}
#endif

#if defined(_WIN32)
static std::string registryString(const char* path, const char* name) {
    char value[256] = {};
    DWORD size = sizeof(value);
    return RegGetValueA(HKEY_LOCAL_MACHINE, path, name, RRF_RT_REG_SZ,
                        nullptr, value, &size) == ERROR_SUCCESS ? trimValue(value) : std::string();
}
#elif !defined(__APPLE__)
static std::string firstLine(const char* path) {
    std::ifstream input(path);
    std::string value;
    std::getline(input, value);
    return trimValue(value);
}
#endif

#if defined(__APPLE__)
static std::string sysctlString(const char* name) {
    size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0) return {};
    std::string value(size, '\0');
    if (sysctlbyname(name, &value[0], &size, nullptr, 0) != 0) return {};
    if (!value.empty() && value.back() == '\0') value.pop_back();
    return value;
}
#endif

SystemInfo querySystemInfo() {
    SystemInfo info;
    info.gpu = rr_platform_renderer_name();
    info.logical_cpus = std::thread::hardware_concurrency();

#ifdef _WIN32
    char hostname[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD hostname_size = sizeof(hostname);
    if (GetComputerNameA(hostname, &hostname_size)) info.hostname = hostname;

    OSVERSIONINFOEXA version = {};
    version.dwOSVersionInfoSize = sizeof(version);
    info.operating_system = "Windows";

    info.cpu = registryString("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                              "ProcessorNameString");

    MEMORYSTATUSEX memory = {};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        info.total_memory_mb = memory.ullTotalPhys / (1024 * 1024);
        info.available_memory_mb = memory.ullAvailPhys / (1024 * 1024);
    }
    const std::string vendor = registryString("HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer");
    const std::string product = registryString("HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName");
    info.model = trimValue(vendor + " " + product);
#elif defined(__APPLE__)
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) == 0) info.hostname = hostname;
    info.model = sysctlString("hw.model");
    info.cpu = sysctlString("machdep.cpu.brand_string");
    if (info.cpu.empty()) info.cpu = sysctlString("hw.model");
    uint64_t memory = 0;
    size_t memory_size = sizeof(memory);
    if (sysctlbyname("hw.memsize", &memory, &memory_size, nullptr, 0) == 0)
        info.total_memory_mb = memory / (1024 * 1024);
    struct utsname os = {};
    if (uname(&os) == 0) info.operating_system = std::string("macOS (Darwin ") + os.release + ")";
#else
    char hostname[256] = {};
    if (gethostname(hostname, sizeof(hostname)) == 0) info.hostname = hostname;
    struct utsname os = {};
    if (uname(&os) == 0) {
        info.operating_system = std::string(os.sysname) + " " + os.release;
        info.model = os.machine;
    }
    const std::string vendor = firstLine("/sys/devices/virtual/dmi/id/sys_vendor");
    const std::string product = firstLine("/sys/devices/virtual/dmi/id/product_name");
    if (!vendor.empty() || !product.empty()) info.model = trimValue(vendor + " " + product);
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        const size_t separator = line.find(':');
        if (separator == std::string::npos) continue;
        const std::string key = trimValue(line.substr(0, separator));
        if (key == "model name" || key == "Hardware") {
            info.cpu = trimValue(line.substr(separator + 1));
            break;
        }
    }
    struct sysinfo memory = {};
    if (sysinfo(&memory) == 0) {
        info.total_memory_mb = (static_cast<uint64_t>(memory.totalram) * memory.mem_unit) / (1024 * 1024);
        info.available_memory_mb = (static_cast<uint64_t>(memory.freeram) * memory.mem_unit) / (1024 * 1024);
    }
#endif

    if (info.hostname.empty()) info.hostname = "Unknown";
    if (info.model.empty()) info.model = "PC";
    if (info.operating_system.empty()) info.operating_system = "Unknown OS";
    if (info.cpu.empty()) info.cpu = "Unknown CPU";
    if (info.gpu.empty()) info.gpu = rr_platform_backend_name();
    return info;
}
