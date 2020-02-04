#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#ifndef HWK
#error *** Need to define a homework number ***
#endif

#if !(defined(ALL) || defined(SINGLE))
#error *** Need to define a submission style ***
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)


const char submit_path_prefix[] = "/c/cs323/Hwk" STR(HWK) "/.submit";

off_t get_regular_file_size(const char *f_name) {
    struct stat info;
    if(stat(f_name, &info) != -1 && (info.st_mode & S_IFMT) == S_IFREG) {
        return info.st_size;
    }
    return -1;
}

int try_submit_file(const char *f_name, const char *submit_path) {
    off_t f_size = get_regular_file_size(f_name);

    if(f_size < 0) {
        return -1;
    }

    char *submit_file_path;
    asprintf(&submit_file_path, "%s/%s", submit_path, f_name);

    int i_fd = open(f_name, O_RDONLY);
    int o_fd = open(submit_file_path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if(i_fd == -1 || o_fd == -1) {
        return -1;
    }

    printf(
        "    [+] Submitted: %s (%i bytes)\n",
        f_name,
        sendfile(o_fd, i_fd, NULL, f_size));

    free(submit_file_path);
    return 0;
}


int main(int argc, char *argv[]) {
    printf(
        "  _  _  _ \n"
        "  _) _) _)\n"
        " __)/____)\n"
        " submission tool\n"
        "\n"
#if defined(ALL)
        "[i] This tool will submit all of the files \n"
        "    in the current directory for your project.\n"
#elif defined(SINGLE)
        "[i] This tool will submit your " STR(SINGLE) " file \n"
        "    in the current directory for your project.\n"
#endif
    );


    /* CREATE USER FOLDER */
    char login_name[64];
    getlogin_r(login_name, sizeof(login_name));
    printf("[i] Submitting for %s.\n", login_name);

    char * submit_path;
    asprintf(&submit_path, "%s/%s", submit_path_prefix, login_name);
    DIR* submit_dir = opendir(submit_path);
    if (submit_dir) {
        closedir(submit_dir);
    } else if (ENOENT == errno) {
        mkdir(submit_path, S_IRWXU | S_IRWXG);
    } else {
        fprintf(stderr, "[!] Failed to create user dir (%i)\n", errno);
    exit(-1);
    }
    free(submit_path);

    /* CREATE SUBMISSION NUM FOLDER */
    int sub_num = 0;
    do {
        asprintf(&submit_path, "%s/%s/%i", submit_path_prefix, login_name, sub_num);

        DIR* submit_dir = opendir(submit_path);
        if (submit_dir) {
            closedir(submit_dir);
            free(submit_path);
        } else if (ENOENT == errno) {
            mkdir(submit_path, S_IRWXU | S_IRWXG);
            printf("[i] Creating submission #%i\n", sub_num);
            break;
        }
    ++sub_num;
    } while(1);


#if defined(ALL)
    DIR* current_dir = opendir(".");
    struct dirent *current_dir_entry;
    while(current_dir != NULL && (current_dir_entry = readdir(current_dir)) != NULL) {
       try_submit_file(current_dir_entry->d_name, submit_path);
    }
#elif defined(SINGLE)
    if(try_submit_file(STR(SINGLE), submit_path)) {
        fprintf(stderr, "[!] Failed to submit file (%i).", errno);
    }
#endif


    return 0;
}
