#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <iostream>
#include <iomanip>
#include <cerrno>
#include <cstdio>
#include <vector>
#include <string>

/******************************************************************************
 * MACROS AND DEFINITIONS
 ******************************************************************************/
#ifndef HWK
#error *** Need to define a homework number ***
#endif

#if !(defined(ALL) || defined(SINGLE))
#error *** Need to define a submission style ***
#endif


#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)



namespace submit {

/******************************************************************************
 * TYPES
 ******************************************************************************/
enum class result_t { success, fail };
enum class create_dir_result_t { created, exists, fail };

/******************************************************************************
 * CONSTANTS
 ******************************************************************************/
static const std::string path_prefix("/c/cs323/Hwk" STR(HWK) "/.submit");


/******************************************************************************
 * PROCEDURES
 ******************************************************************************/
bool is_file_allowed(const std::string & f_name) {
    static const std::vector<std::string> disallowed_suffix = {
        ".o",
        ".swp",
        ".out",
    };

    static const std::vector<std::string> disallowed_substring = {
        "vgcore.",
        ".core.",
    };

    for(auto it = disallowed_suffix.begin(); it != disallowed_suffix.end(); ++it) {
        if(f_name.compare(f_name.size()-(*it).size(), (*it).size(), (*it)) == 0) {
            return false;
        }
    }

    for(auto it = disallowed_substring.begin(); it != disallowed_substring.end(); ++it) {
        if(f_name.find(*it) != std::string::npos) {
            return false;
        }
    }

    return true;
}



off_t get_regular_file_size(const std::string & f_name) {
    struct stat info;
    if(stat(f_name.c_str(), &info) != -1 && (info.st_mode & S_IFMT) == S_IFREG) {
        return info.st_size;
    }
    return -1;
}

time_t get_modified_time_sec(const std::string &path) {
    struct stat info;
    if(stat(path.c_str(), &info) != -1) {
        return info.st_mtim.tv_sec;
    }
    return 0;
}


result_t try_submit_file(const std::string & f_name, const std::string & submit_path) {
    off_t f_size = get_regular_file_size(f_name);

    if(f_size < 0) {
        return result_t::fail;
    }

    if(!is_file_allowed(f_name)) {
        std::cout << "    [i] Skipping: " << f_name << ", disallowed file type.\n";
        return result_t::fail;
    }

    std::string submit_file_path = submit_path + "/" + f_name;

    int i_fd = open(f_name.c_str(),
            O_RDONLY);
    int o_fd = open(submit_file_path.c_str(),
            O_CREAT|O_WRONLY|O_TRUNC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if(i_fd == -1 || o_fd == -1) {
        std::cerr << "    [!] Error: " << f_name << ", unable to open file.\n";
        return result_t::fail;
    }

    auto submitted_size = sendfile(o_fd, i_fd, NULL, f_size);

    close(i_fd);
    close(o_fd);

    if(submitted_size != f_size) {
        std::cerr << "    [!] Error: " << f_name << ", failed to copy file\n";
        return result_t::fail;
    } else {
        std::cout << "    [i] Submitted: " << f_name << " (" << submitted_size << " bytes)\n";
        return result_t::success;
    }
}



std::string get_user() {
    char login_name[64];
    getlogin_r(login_name, sizeof(login_name));

    return std::string(login_name);
}


create_dir_result_t create_dir(const std::string &path) {

    DIR* dir = opendir(path.c_str());
    if (dir) {
        closedir(dir);
        return create_dir_result_t::exists;
    } else if (ENOENT == errno) {
        mkdir(path.c_str(), S_IRWXU | S_IRWXG);
        return create_dir_result_t::created;
    } else {
        return create_dir_result_t::fail;
    }

}



void print_files_in_dir(const std::string &path) {
    DIR* current_dir = opendir(path.c_str());
    struct dirent *current_dir_entry;
    while(current_dir != NULL && (current_dir_entry = readdir(current_dir)) != NULL) {
        std::string f_name(current_dir_entry->d_name);

        off_t f_size = get_regular_file_size(f_name);
        time_t mod_time = get_modified_time_sec(f_name);

        if(f_size == -1) continue;

        std::cout << "    [-] " << f_name <<
            std::put_time(std::localtime(&mod_time), " (modified: %b %e %I:%M %p)") << "\n";
    }
}


} /* namespace submit */

int main(int argc, char *argv[]) {
    std::cout <<
        "  _  _  _ \n"
        "  _) _) _)\n"
        " __)/____)\n"
        " submission tool\n"
        "\n"
#if defined(ALL)
        "[i] This tool will submit ALL of the files \n"
#elif defined(SINGLE)
        "[i] This tool will submit your " STR(SINGLE) " file \n"
#endif
        "    in the current directory for your project.\n"
        "    You may submit any number of times, and \n"
        "    more intermediate submissions are\n"
        "    encouraged.\n";


    /* CREATE USER FOLDER */
    std::string user_name = submit::get_user();
    std::string user_submit_path = submit::path_prefix + "/" + user_name;

    std::cout << "[i] Submitting for " << user_name << ".\n";

    if(submit::create_dir(user_submit_path) == submit::create_dir_result_t::fail) {
        std::cerr << "[!] Failed to create user dir.\n";
        return 1;
    }


    /* CREATE SUBMISSION NUM FOLDER */
    std::string user_submit_num_path;
    int sub_num = 0;
    do {
        user_submit_num_path = user_submit_path + "/" + std::to_string(sub_num);

        if(submit::create_dir(user_submit_num_path) == submit::create_dir_result_t::created) {
            std::cout << "[i] Creating submission #" << sub_num << "\n";
            break;
        }

        ++sub_num;
    } while(1);


    int submission_count = 0;

#if defined(ALL)
    DIR* current_dir = opendir(".");
    struct dirent *current_dir_entry;
    while(current_dir != NULL && (current_dir_entry = readdir(current_dir)) != NULL) {
        if(submit::try_submit_file(
                std::string(current_dir_entry->d_name),
                user_submit_num_path) == submit::result_t::success) {
            submission_count++;
        }
    }
#elif defined(SINGLE)
    if(submit::try_submit_file(STR(SINGLE), user_submit_num_path) == submit::result_t:fail) {
        std::cerr << "[!] Failed to submit assignment.\n";
        return 1;
    }
    submission_count++;
#endif

    std::cout << "[i] Completed submission of " << submission_count << " files:\n";
    submit::print_files_in_dir(user_submit_num_path);

    return 0;
}


