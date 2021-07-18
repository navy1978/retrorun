#pragma once

#include <string>
#include <map>

std::map<std::string, std::string> initMapConfig(std::string pathConfFile);

std::map<std::string, std::string> getConfigMap();

void getConfParameter(const std::string &findMe, char *result);
