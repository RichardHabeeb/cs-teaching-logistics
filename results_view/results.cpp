#include <iostream>
#include <fstream>
#include <unistd.h>
#include <fcntl.h>

/******************************************************************************
 * MACROS AND DEFINITIONS
 ******************************************************************************/
#ifndef RESULTS_PATH
#error *** Need to define a RESULTS_PATH ***
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/******************************************************************************
 * CONSTANTS
 ******************************************************************************/
static const std::string path_prefix(STR(RESULTS_PATH));


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
    int sub_num = 0;

    std::string path_with_net_id = path_prefix + "/" + net_id + "-";

    while(faccessat(0, (path_with_net_id + std::to_string(sub_num) + ".txt").c_str(), F_OK, AT_EACCESS) == -1) {
        if(++sub_num > 1024*16) {
            std::cout << "No results log found for " << net_id << "\n";
            return -1;
        }
    }

    std::ifstream log_file;
    log_file.open(path_with_net_id + std::to_string(sub_num) + ".txt");
    std::cout << log_file.rdbuf();
    log_file.close();
    return 0;
}
