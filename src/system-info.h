#pragma once

#include <stdint.h>
#include <string>

struct SystemInfo {
    std::string hostname;
    std::string model;
    std::string operating_system;
    std::string cpu;
    std::string gpu;
    unsigned logical_cpus = 0;
    uint64_t total_memory_mb = 0;
    uint64_t available_memory_mb = 0;
};

SystemInfo querySystemInfo();
