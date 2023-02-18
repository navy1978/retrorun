#include <iostream>
#include <regex>



static std::string trim(std::string str)
{
    return regex_replace(str, std::regex("(^[ ]+)|([ ]+$)"), "");
}


int main() {

    char t[] = "      C++";
std::cout << t;
    std::cout << trim(t);
    return 0;
}

/**
 * TODO:
 * 
 * revove the double images size (we can keep only one)
 * clean up the code

 * 
 * 
*/