#include <conf.h>
#include <fstream>
#include <iostream>
#include <utility>
#include <string>
#include <map>
#include <sstream>

const char* ws = " \t\n\r\f\v";

// trim from end of string (right)
inline std::string& rtrim(std::string& s)
{
    s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

// trim from beginning of string (left)
inline std::string& ltrim(std::string& s)
{
    s.erase(0, s.find_first_not_of(ws));
    return s;
}

// trim from both ends of string (right then left)
inline std::string& trim(std::string& s)
{
    return ltrim(rtrim(s));
}


/**
 * Read a config file passed as parameter and create a map with all entries < key = value >
 * */

std::map<std::string, std::string> mapConfigFile(std::string pathConfFile){
    std::ifstream file_in(pathConfFile);
    // std::cout << file_in.rdbuf(); // debug    
    std::map<std::string, std::string> conf_map;
    std::string key;
    std::string value;

    while(std::getline(file_in, key, '=') && std::getline(file_in, value))
    {
        try{
        std::size_t pos_sharp = key.find("#");
        if (pos_sharp == 0) {
            
            key = key.substr (key.find("\n")+1, key.length());
            std::istringstream iss(key);
            std::getline(iss, key, '=');
            std::getline(iss, value);
        }
        key = trim (key);
        value = trim (value);
        conf_map.insert ( std::pair<std::string,std::string>(key,value) );
        }catch(...){
             std::cout << "Error reading configuration file, key: "<< key << "\n";
        }
    }
    return conf_map;
}