#include <iostream>
#include <fstream>
#include <unistd.h>

std::string get_user() {
    char login_name[64];
    getlogin_r(login_name, sizeof(login_name));

    return std::string(login_name);
}

int main() {

    std::string net_id = get_user();
    int sub_num = 0;

    while(access(("/c/cs323/Hwk1/.result/"
                    + net_id + "-"
                    + std::to_string(sub_num)
                    + ".txt").c_str(), F_OK) == -1 && sub_num < 1024*1024) {
        ++sub_num;
    }


    std::ifstream myfile;
    myfile.open("/c/cs323/Hwk1/.result/" + net_id + "-" + std::to_string(sub_num) + ".txt");
    std::cout << myfile.rdbuf();
    myfile.close();
    return 0;
}
