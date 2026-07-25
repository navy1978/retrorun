#pragma once

#include <string>

void decoration_catalog_init();
void decoration_catalog_update();
void decoration_catalog_shutdown();

std::string decoration_catalog_pack_label();
std::string decoration_catalog_source_label();
std::string decoration_catalog_active_label();
std::string decoration_catalog_status_label();
void decoration_catalog_select_source(int button);
void decoration_catalog_select(int button);
void decoration_catalog_install();
void decoration_catalog_remove();
