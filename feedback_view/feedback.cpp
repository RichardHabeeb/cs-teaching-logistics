#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>

/******************************************************************************
 * MACROS AND DEFINITIONS
 ******************************************************************************/
#ifndef FEEDBACK_PATH
#error *** Need to define a FEEDBACK_PATH ***
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/******************************************************************************
 * CONSTANTS
 ******************************************************************************/
static const std::string path_prefix(STR(FEEDBACK_PATH));


/******************************************************************************
 * PROCEDURES
 ******************************************************************************/
std::string get_user() {
    char login_name[64];
    getlogin_r(login_name, sizeof(login_name));

    return std::string(login_name);
}

int main() {

    std::string net_id = get_user();
    std::string path_with_net_id = path_prefix + "/" + net_id + ".txt";

    if(faccessat(0, path_with_net_id.c_str(), F_OK, AT_EACCESS) == -1)
    {
        std::cerr << "No results log found for " << net_id << "\n";
        return -1;
    }

    std::ifstream log_file;
    log_file.open(path_with_net_id);
    std::cout << log_file.rdbuf();
    log_file.close();
    return 0;
}
