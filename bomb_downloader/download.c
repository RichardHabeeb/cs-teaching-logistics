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
#include <string.h>

#include "colors.h"

#define HWK 3

#ifndef HWK
#error *** Need to define a homework number ***
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

//#define DEBUG

#ifdef DEBUG
#define DEBUGLOG(l, ...){               \
	fprintf(stderr, __FILE__ ": "); \
	fprintf(stderr, __VA_ARGS__);   \
	fprintf(stderr, "\n");          \
	}
#else
#define DEBUGLOG(l,...) {} /*DISABLED*/
#endif

#ifdef DEBUG
const char bomb_path_prefix[] = "/home/accts/ks2446/c/cs323/Hwk" STR(HWK) "/bombs/";
const char home_path_prefix[] = "/home/accts/";
const char bomb_file_prefix[] = "bomb";
const char bomb_file_suffix[] = ".tar";
#else
const char bomb_path_prefix[] = "/c/cs323/Hwk" STR(HWK) "/.bombs/";
const char home_path_prefix[] = "/home/accts/";
const char bomb_file_prefix[] = "bomb";
const char bomb_file_suffix[] = ".tar";
#endif

int check_file_name(const char * name){
	int len = strlen(name);
	int len2 = strlen(bomb_file_prefix);
	for(int i = 0 ; i < len && i < len2 ; i++){
		if(name[i] != bomb_file_prefix[i])
			return 0;
	}
	len2 = strlen(bomb_file_suffix);
	for(int  i = 0 ; len2 - i - 1 >= 0 && len - i - 1 >= 0 ; i++){
		if(name[len - i - 1] != bomb_file_suffix[len2 - i - 1])
			return 0;
	}
	DEBUGLOG(1, "I think %s is a bomb*.tar file\n", name);
	return 1;
}


off_t get_regular_file_size(const char *f_name) {
    struct stat info;
    if(stat(f_name, &info) != -1 && (info.st_mode & S_IFMT) == S_IFREG) {
        return info.st_size;
    }
    return -1;
}

int try_submit_file(const char *src_file, const char *dest_file) {
    off_t f_size = get_regular_file_size(src_file);
   
    if(f_size < 0) {
        return -1;
    }

    int i_fd = open(src_file, O_RDONLY);
    int o_fd = open(dest_file, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if(i_fd == -1 || o_fd == -1) {
        return -1;
    }

    printf(
        "[+] Downloaded: %s (%ld bytes)\n",
        dest_file,
        sendfile(o_fd, i_fd, NULL, f_size));
    return 0;
}


int main(int argc, char *argv[]) {
    printf(
	"  )\n"
	"  (\n"
	" [" BROWN "_" RESET "]ↄ\n"
        "\n"
	"This tool will download HWK " STR(HWK) " in your class directory!\n"
    );


    /* CREATE USER FOLDER */
    char login_name[64];
    getlogin_r(login_name, sizeof(login_name));
    printf("[i] Downloading for %s.\n", login_name);
    
    char * bomb_path;
    asprintf(&bomb_path, "%s/%s/", bomb_path_prefix, login_name);
    
    DIR* bomb_dir = opendir(bomb_path);
    struct dirent *bomb_ptr;

    DEBUGLOG(1, "Dir: %s", bomb_path);

    if (bomb_dir) {
	    while((bomb_ptr = readdir(bomb_dir)) != NULL){
		    if(check_file_name(bomb_ptr->d_name)){
			break;
		    }
	    }
    } else {
	fprintf(stderr, RED "Error! Could not find a bomb for you! Please contact TF Karthik Sriram with your netid\n" RESET);
	exit(1);
    }
    if(!bomb_ptr){
	fprintf(stderr, RED "Error! Could not find a bomb for you! Please contact TF Karthik Sriram with your netid\n" RESET);
	exit(1);
    }
    char * bomb;
    asprintf(&bomb, "%s/%s", bomb_path, bomb_ptr->d_name);

    char * dest_path;
    asprintf(&dest_path, "%s/%s/cs323/", home_path_prefix, login_name);

    char * dest_file;
    asprintf(&dest_file, "%s/%s", dest_path, bomb_ptr->d_name);

    if(get_regular_file_size(dest_file) > 0){
	fprintf(stderr, RED "Error. File %s exists in %s. Are you sure you want to overwrite[y/n]?: " RESET, bomb_ptr->d_name ,dest_path);
	char response = 'd';
	scanf("%c", &response);
	switch(response){
		case 'y':
		case 'Y':
			
			break;
		case 'n':
		case 'N':
		default:
			printf(BOLDGREEN "Did not copy bomb for %s\n" RESET, login_name);
			exit(0);
			break;
	}
    }	

    if(try_submit_file(bomb, dest_file)) {
        fprintf(stderr, RED "[!] Failed to create file (%i).", errno);
	fprintf(stderr, RED "Please contact TF Karthik Sriram with your netid and errno (%i)" RESET, errno);
    }
    return 0;
}
