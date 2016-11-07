#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "common.h"
#include "cont_list.h"

using namespace std;

int main(int argc, char *argv[]) {
    int id = atoi(argv[1]);
    int signum = SIGTERM;
    if (argc > 2) {
        signum = atoi(argv[2]);
    }
    string cmd = "sudo kill -" + to_string(signum) + " " + to_string(id);
    check_res(system(cmd.c_str()), "Error in stopping container");

    remove_cont(id);
    return 0;
}
