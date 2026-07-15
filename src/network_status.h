#pragma once

#include <string>

void network_status_refresh();
void network_status_shutdown();
std::string network_status_connection_label();
std::string network_status_interface_label();
std::string network_status_address_label();
std::string network_status_latency_label();
std::string network_status_checked_label();
