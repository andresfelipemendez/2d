#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>
#include <ftw.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

// keep two arrays, one of time stamps one of hash values
// if time stamps differ then compare the hashed value
// if the hash is different a file was deleted
// if it matches then it was modified

long time_stamps[512];
long hashes[512];
struct stat buff;
int file_changed = 0;
char name[512];
pid_t game_pid = -1;
int current_file_index = 0;

const char* ignore_watch_dirs[] = {
	".git",
	"build",
	"libs",
	NULL
};

const char* src_files[] = {
    "main.c",
    NULL
};

const char* include_dirs[] = {
	"libs/SDL3/include",
	NULL
};

const char* lib_dirs[] = {
	"libs/SDL3/lib/libSDL3.a",
	NULL
};

const char*	libraries[] = {
	"m", "dl", "pthread",
    "wayland-client", "wayland-cursor", "wayland-egl",
    "xkbcommon", "decor-0",
    "asound", "pulse", "udev", "drm", "gbm", "EGL", "GL",
    "X11", "Xext", "Xrandr", "Xi", "Xfixes", "Xcursor", "Xss",
	NULL
};

/*	
int hash (char*) {
}
*/

bool has_extension(const char *filename, const char *extension){
	const char *dot = strrchr(filename,'.');
	if (dot == NULL || dot[1] == '\0') {
		return false;
	}
	return strcmp(dot + 1, extension) == 0;
}

int display_info(const char *fpath, const struct stat *sb, int typeflag) {
	if(strcmp(".", fpath) == 0) {
		return 0;
	}

	const char* path_to_check = fpath;
	if(strncmp(path_to_check, "./", 2) == 0) {
		path_to_check += 2;
	}

	for(int i = 0; ignore_watch_dirs[i] != NULL; i++) {
		char skipped_dir_name_len = strlen(ignore_watch_dirs[i]);
		char *dir_pos = strstr(path_to_check, ignore_watch_dirs[i]);
		if(dir_pos != NULL) {
			return 0;
		}
	}

	int file_index = current_file_index++;
	if(stat(fpath,&buff) == 0) {
		if(time_stamps[file_index] != buff.st_mtime) {
			if(has_extension(fpath, "c")){
			strcpy(name,fpath);
			time_stamps[file_index] = buff.st_mtime;
			file_changed = 1;
			return 1;
			}
		}
	}
	return 0;
}

void concat_list(char* compile_cmd, char* prefix,const char** list) {
	for(int i = 0; list[i] != NULL; i++) {
		strcat(compile_cmd, " ");
		if(prefix != NULL) {
			strcat(compile_cmd, prefix);
		}
		strcat(compile_cmd, list[i]);
	}
}

void kill_game_process() {
	if(game_pid > 0) {
        printf("Killing process %d...\n", game_pid);
		kill(game_pid, SIGTERM);

		int status;
		int time_out = 0;
		while (time_out < 50) {
			if(waitpid(game_pid, &status, WNOHANG) > 0) {
				printf("Process terminated gracefully\n");
				game_pid = -1;
				return;
			}
			usleep(10000);
			time_out++;			
		}
		
		if(kill(game_pid, 0) == 0) {
			printf("Force killing process...\n");
			kill(game_pid, SIGKILL);
			waitpid(game_pid, &status, 0);
		}
		game_pid = -1;
	}
}

int main() {

	char compile_cmd[1024] = "gcc -o sdlengine";
    concat_list(compile_cmd, "", src_files);
	concat_list(compile_cmd, "-I", include_dirs);
	concat_list(compile_cmd, "", lib_dirs);
	concat_list(compile_cmd, "-l",libraries);

	printf("compile command: %s\n", compile_cmd);
	while(true) {
		file_changed = 0;
		current_file_index = 0;

		ftw(".", display_info, 20);

		if(file_changed) {
			char *time_str = ctime(&buff.st_mtime);
			time_str[strlen(time_str) - 1] = '\0';
			printf("changed %s %s\n",time_str, name);

			kill_game_process();
			int result = system(compile_cmd);
			if (result==0) {
				game_pid = fork();
				if(game_pid==0) {
					execl("./sdlengine", "./sdlengine",NULL);
					perror("Failed to start server");
				} else if(game_pid < 0) {
					perror("Failed to fork server");
				}
			}
			else {
				puts("build failed");
			}
		}
		usleep(10000);
	}
}
