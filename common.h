#pragma once

#include <fstream>
#include <err.h>
#include <dirent.h>
#include <sys/stat.h>

using namespace std;

#define check_res(value, message) \
if (value < 0){ \
    printf("%s error: %d %s \n", message, value, strerror(errno)); \
    exit(value); \
} \


void set_map(char *file, int inside_id, int outside_id, int len) {
    FILE *mapfd = fopen(file, "w");
    if (NULL == mapfd) {
        perror("open file error");
        printf("set_map error %s", file);
        exit(0);
    }
    fprintf(mapfd, "%d %d %d\n", inside_id, outside_id, len);
    fclose(mapfd);
}

void set_uid_map(pid_t pid, int inside_id, int outside_id, int len) {
    char file[256];
    sprintf(file, "/proc/%d/uid_map", pid);
    set_map(file, inside_id, outside_id, len);
}

void set_gid_map(pid_t pid, int inside_id, int outside_id, int len) {
    char file[256];
    sprintf(file, "/proc/%d/gid_map", pid);
    set_map(file, inside_id, outside_id, len);
}
