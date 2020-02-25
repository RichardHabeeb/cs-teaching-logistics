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
#include <filesystem>

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
static const std::string home_prefix("/home/accts");

static const off_t max_allowed_file_size = 1*1024*1024;
static const std::vector<std::string> disallowed_suffix = {
    ".o",
    ".swp",
    ".out",
".exe",
};

static const std::vector<std::string> disallowed_substring = {
    "vgcore.",
    ".core.",
};


/******************************************************************************
 * PROCEDURES
 ******************************************************************************/
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

bool is_file_allowed(const std::string & f_name) {

    for(auto it = disallowed_suffix.begin(); it != disallowed_suffix.end(); ++it) {
        if((*it).size() > f_name.size()) continue;
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



result_t try_submit_file(const std::string & f_name, const std::string & submit_path) {
    off_t f_size = get_regular_file_size(f_name);

    if(f_size < 0) {
        return result_t::fail;
    }

    if(!is_file_allowed(f_name)) {
        std::cout << "    [i] Skipping: " << f_name << ", disallowed file type.\n";
        return result_t::fail;
    }

    if(f_size > max_allowed_file_size) {
        std::cout
            << "    [i] Skipping: " << f_name << ", file too large (max : "
            << max_allowed_file_size << ")\n";
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
        std::cout << "    [-] Submitted: " << f_name << " (" << submitted_size << " bytes)\n";
        return result_t::success;
    }
}


off_t try_copy_file(const std::string &src_path, const std::string &dst_path) {

    off_t f_size = get_regular_file_size(src_path.c_str());

    if(f_size < 0) {
        return -1;
    }

    int i_fd = open(src_path.c_str(),
            O_RDONLY);
    int o_fd = open(dst_path.c_str(),
            O_CREAT|O_WRONLY|O_TRUNC,
            S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
    if(i_fd == -1 || o_fd == -1) {
        return -1;
    }

    auto submitted_size = sendfile(o_fd, i_fd, NULL, f_size);

    close(i_fd);
    close(o_fd);

    return submitted_size;
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
    } else if (ENOENT == errno && mkdir(path.c_str(), S_IRWXU | S_IRWXG) == 0) {
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

        off_t f_size = get_regular_file_size(path + "/" + f_name);
        time_t mod_time = get_modified_time_sec(path + "/" + f_name);

        if(f_size == -1) continue;

        std::cout << "    [-] " << f_name <<
            std::put_time(std::localtime(&mod_time), " (submitted on: %b %e %I:%M %p)") << "\n";
    }

    if(current_dir == NULL) {
        std::cerr << "[!] Failed to access directory (" << errno << ")\n";
    }
}


int next_submission_number(const std::string &user_submit_path) {
    int sub_num = 0;
    std::string user_submit_num_path;
    do {
        user_submit_num_path = user_submit_path + "/" + std::to_string(sub_num);

        DIR* dir = opendir(user_submit_num_path.c_str());
        if (dir) {
            closedir(dir);
        } else if (ENOENT == errno) {
            return sub_num;
        }

        ++sub_num;
    } while(1);
}


result_t do_submission(const std::string &user_name, const std::string &user_submit_path) {

    std::cout << "[i] Submitting for " << user_name << ".\n";

    /* CREATE SUBMISSION NUM FOLDER */
    int sub_num = next_submission_number(user_submit_path);
    std::string user_submit_num_path = user_submit_path + "/" + std::to_string(sub_num);

    std::cout << "[i] Creating submission #" << sub_num << "\n";
    if(create_dir(user_submit_num_path) != create_dir_result_t::created) {
        std::cerr << "[!] Error: Failed to create submission dir.\n";
        return result_t::fail;
    }

    int submission_count = 0;

#if defined(ALL)
    DIR* current_dir = opendir(".");
    struct dirent *current_dir_entry;
    while(current_dir != NULL && (current_dir_entry = readdir(current_dir)) != NULL) {
        if(try_submit_file(
                std::string(current_dir_entry->d_name),
                user_submit_num_path) == result_t::success) {
            submission_count++;
        }
    }
#elif defined(SINGLE)
    if(try_submit_file(STR(SINGLE), user_submit_num_path) == result_t::fail) {
        std::cerr << "[!] Error: Failed to submit assignment.\n";
        return result_t::fail;
    }
    submission_count++;
#endif

    if(submission_count == 0) {
        std::cerr << "[!] Error: No files to submit.\n";
        (void) rmdir(user_submit_num_path.c_str());
        return result_t::fail;
    }

    std::cout << "[i] Completed submission of " << submission_count << " files:\n";
    print_files_in_dir(user_submit_num_path);

    return result_t::success;
}



std::string stage_files(const std::string &user_name) {
    char path_template[] = "/tmp/cs323-stage-XXXXXX";
    std::string tmp_dir(mkdtemp(path_template));

#if defined(ALL)
    DIR* current_dir = opendir(".");
    struct dirent *current_dir_entry;
    while(current_dir != NULL && (current_dir_entry = readdir(current_dir)) != NULL) {
        if(try_submit_file(
                std::string(current_dir_entry->d_name),
                tmp_dir) == result_t::success) {
        }
    }
#elif defined(SINGLE)
    if(try_submit_file(STR(SINGLE), tmp_dir) == result_t::fail) {
        std::cerr << "[!] Error: Failed to submit assignment.\n";
        return std::string();
    }
#endif

    std::cout << "[i] Staged files to " << tmp_dir << "\n";


    return tmp_dir;

}


//void do_safe_compilation() {
//
//    pid_t pid = fork();
//    if(pid == 0) {
//        /* De escalate permissions */
//        setgid(getuid());
//
//        (void) execl("make", NULL);
//        std::cerr << "[!] Failed to exec make\n";
//        exit(1);
//
//    } else {
//    }
//}


result_t do_retrieval(const std::string &user_name, const std::string &user_submit_path, int sub_num) {
    std::string user_submit_num_path = user_submit_path + "/" + std::to_string(sub_num);
    std::string home_dir_dst = home_prefix + "/" + user_name + "/retrieved-Hwk1-sub-" + std::to_string(sub_num);

    std::cout << "[i] Creating retrieval folder: " << home_dir_dst << "\n";
    if(create_dir(home_dir_dst) != create_dir_result_t::created) {
        std::cerr << "[!] Error: Failed to create retrieval dir, check to see if it already exists.\n";
        return result_t::fail;
    }
    (void) chown(home_dir_dst.c_str(), getuid(), getuid());

    DIR* current_dir = opendir(user_submit_num_path.c_str());
    struct dirent *current_dir_entry;
    while(current_dir != NULL && (current_dir_entry = readdir(current_dir)) != NULL) {

        off_t size = try_copy_file(
                user_submit_num_path + "/" + current_dir_entry->d_name,
                home_dir_dst + "/" + current_dir_entry->d_name);

        if(size != -1) {
            std::cout << "    [-] Retrieved: " << current_dir_entry->d_name << "\n";
            (void) chown((home_dir_dst + "/" + current_dir_entry->d_name).c_str(), getuid(), getuid());
        }
    }

    if(current_dir == NULL) return result_t::fail;
    return result_t::success;
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
        "    encouraged.\n"
        "[i] usage: /c/cs323/Hwk" STR(HWK) "/submit [info|get [N]]\n"
        "    [-] info will list the files submitted for\n"
        "        the Nth or latest submission\n"
        "    [-] get will retrieve the files submitted\n"
        "        for the Nth or latest submission.\n";


    /* CREATE USER FOLDER */
    std::string user_name = submit::get_user();
    std::string user_submit_path = submit::path_prefix + "/" + user_name;

    if(submit::create_dir(user_submit_path) == submit::create_dir_result_t::fail) {
        std::cerr << "[!] Error: Failed to create user dir.\n";
        return 1;
    }


    if(argc > 1 && std::string(argv[1]) == "info") {
        int sub_num;
        if(argc == 3) {
            sub_num = std::max(0, std::stoi(std::string(argv[2])));
        } else {
            sub_num = std::max(0, submit::next_submission_number(user_submit_path) - 1);
        }

        std::string user_submit_num_path = user_submit_path + "/" + std::to_string(sub_num);

        std::cout << "[i] Information about submission #" << sub_num << " from " << user_name << ".\n";
        std::cout << "[i] Files submitted:\n";
        submit::print_files_in_dir(user_submit_num_path);

    } else if(argc > 1 && std::string(argv[1]) == "get") {
        int sub_num;
        if(argc == 3) {
            sub_num = std::max(0, std::stoi(std::string(argv[2])));
        } else {
            sub_num = std::max(0, submit::next_submission_number(user_submit_path) - 1);
        }

        std::string user_submit_num_path = user_submit_path + "/" + std::to_string(sub_num);

        if(submit::do_retrieval(user_name, user_submit_path, sub_num) == submit::result_t::fail) {
            return 1;
        }


    } else {

        (void) submit::stage_files(user_name);

        if(submit::do_submission(user_name, user_submit_path) == submit::result_t::fail) {
            return 1;
        }
    }



    return 0;
}


