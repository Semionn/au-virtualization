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

    string cgroup_base_dir = "/tmp/cgroup";
    string system_path = cgroup_base_dir + "/cpu";
    string cg_path = system_path + "/" + to_string(pid);

    string s3 = "echo " + to_string(getpid()) + " >> " + cg_path + "/tasks";
    check_res(system(s3.c_str()), "echo tasks");

    vector<string> nsnames = {"user", "uts", "ipc", "net", "pid", "mnt"};
    for (string &ns_name : nsnames) {
        string ns_file_path_str = "/proc/" + to_string(pid) + "/ns/" + ns_name;
        int ns_fd = open(ns_file_path_str.c_str(), O_RDONLY, O_CLOEXEC);
        check_res(ns_fd, "ns open");
        check_res(setns(ns_fd, 0), "setns");
        check_res(close(ns_fd), "ns close");
    }

    check_res(chdir("/bin"), "chdir");

    int child_pid = fork();
    check_res(chdir("/"), "chdir");
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
