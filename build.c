#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
//#include <errno.h>
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

typedef struct{
	const char** src_files;
	const char** include_dirs;
	const char** lib_files;
	const char** libraries;
	const char* output_name;
	const char* extra_flags;
	bool is_shared_lib;
} BuildConfig;

long time_stamps[512];
long hashes[512];
struct stat buff;
int file_changed = 0;
char name[512];
pid_t game_pid = -1;
int current_file_index = 0;
bool main_app_built = false;

const char* ignore_watch_dirs[] = {
	".git",
	"build",
	"libs",
	NULL
};

const char* main_src_files[] = {
    "main.c",
    NULL
};

const char* engine_src_files[] = {
	"engine.c",
	"libs/glad/glad.c",
	NULL
};

const char* include_dirs[] = {
	"libs/SDL3/include",
	NULL
};

const char* engine_include_dirs[] = {
	"libs/SDL3/include",
	"libs/glad",
	NULL
};

const char* lib_files[] = {
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

const char* engine_libraries[] = {
	"GL", "m",
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

bool build_targe(const BuildConfig* config) {
	char compile_cmd[2048] = "gcc -std=c99 -Wall -Wextra -g -O0";

	if(config->is_shared_lib) {
		strcat(compile_cmd, " -fPIC -shared");
	}

	if(config->extra_flags) {
		strcat(compile_cmd, " ");
		strcat(compile_cmd, config->extra_flags);
	}

	strcat(compile_cmd, " -o ");
	strcat(compile_cmd, config->output_name);

	concat_list(compile_cmd, " ", config->src_files);
	concat_list(compile_cmd, "-I", config->include_dirs);

	if(config->lib_files) {
		concat_list(compile_cmd, " ", config->lib_files);
	}

	concat_list(compile_cmd, " -l", config->libraries);
	
	printf("Building: %s\n",compile_cmd);
	int result = system(compile_cmd);

	if(result == 0) {
        printf("✓ %s built successfully\n", config->output_name);
        return true;
    } else {
        printf("✗ %s build failed\n", config->output_name);
        return false;
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

void start_main_app() {
	printf("Starting hot reload engine\n");
	game_pid = fork();
	if(game_pid == 0) {
		execl("./hot_reload_engine", "./hot_reload_engine", NULL);
        perror("Failed to start hot reload engine");
        exit(1);
    } else if(game_pid < 0) {
        perror("Failed to fork");
        game_pid = -1;
    } else {
        printf("Started with PID %d\n", game_pid);
    }
}
bool build_main_app() {
	BuildConfig main_config = {
		.src_files = main_src_files,
		.include_dirs = include_dirs,
		.lib_files = lib_files,
		.libraries = libraries,
		.output_name = "hot_reload_engine",
		.extra_flags = NULL,
		.is_shared_lib = false
	};

	return build_targe(&main_config);
}

bool build_engine() {
	BuildConfig main_config = {
		.src_files = engine_src_files,
		.include_dirs = engine_include_dirs,
		.lib_files = NULL,
		.libraries = engine_libraries,
		.output_name = "libengine.so",
		.extra_flags = NULL,
		.is_shared_lib = true
	};

	return build_targe(&main_config);
}

int main() {
	if(!build_main_app()) {
		printf("Failed to build main application.\n");
		return 1;
	}
	main_app_built = true;

	if(!build_engine()){
		printf("Failed to build engine library.\n");
		return 1;
	}

	start_main_app();
	
	while(true) {
		file_changed = 0;
		current_file_index = 0;

		ftw(".", display_info, 20);

		if(file_changed) {
			char *time_str = ctime(&buff.st_mtime);
			time_str[strlen(time_str) - 1] = '\0';
			printf("\n=== File changed: %s at %s ===\n", name, time_str);

			if(strstr(name,"main.c") != NULL){
				kill_game_process();
				if(build_main_app()) {
					build_engine();
					start_main_app();
				} else {
					printf("Main app build failed, not restarting\n");
				}
			} else if (strstr(name, "engine.c") != NULL) {
				printf("Engine source changed, rebuilding library for hot reload...\n");
				if(build_main_app()) {
					printf("Engine rebuilt! Hot reload should happen automatically.\n");
				} else {
					printf("Main app build failed, not restarting\n");
				}
			}
		}
		usleep(10000);
	}
	return 0;
}
