#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <vector>
#include <string.h>
#include <grp.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include "common.h"

using namespace std;

int pipe_fd[2];

int main(int argc, char *argv[]) {
    using namespace std;
    int pid = atoi(argv[1]);

    pipe(pipe_fd);

    const int gid = getgid(), uid = getuid();
    string root_path = "/proc/" + to_string(pid) + "/root";
    int fd = open(root_path.c_str(), O_RDONLY);
    check_res(fchdir(fd), "fchdir");

    vector<string> nsnames = {"user", "uts", "ipc", "net", "pid", "mnt"};
    for (string &ns_name : nsnames) {
        string ns_file_path_str = "/proc/" + to_string(pid) + "/ns/" + ns_name;
        int ns_fd = open(ns_file_path_str.c_str(), O_RDONLY, O_CLOEXEC);
        check_res(ns_fd, "ns open");
        check_res(setns(ns_fd, 0), "setns");
        check_res(close(ns_fd), "ns close");
    }

    check_res(chroot("."), "chroot");
    check_res(chdir("/bin"), "chdir");

    int child_pid = fork();
    if (child_pid == 0) {
        setgroups(0, NULL);
        setgid(0);
        setuid(0);
        check_res(execvp(argv[2], &argv[2]), "execvp");
    } else {
        close(pipe_fd[1]);
        waitpid(child_pid, NULL, 0);
    }
    return 0;
}
