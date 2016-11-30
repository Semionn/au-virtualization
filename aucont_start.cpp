#include <iostream>
#include <string.h>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/mount.h>
#include <grp.h>
#include <sys/capability.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <bits/unique_ptr.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <thread>
#include <arpa/inet.h>
#include "common.h"
#include "cont_list.h"

using namespace std;

const char* CONT_HOSTNAME = "container";

#define STACK_SIZE (1024 * 1024)
static char container_stack[STACK_SIZE];

class Arguments {
public:
    int pid;
    int cpu_perc = 100;
    bool daemonize = false;
    string image_path = "";
    string cmd = "";
    vector<char *> args;
    string net = "";
    string net_id = "";

    int pipe_fd[2];
};

char* next_addr(string address_string) {
    in_addr_t address = inet_addr(address_string.c_str());

    address = htonl(ntohl(address) + 1);

    struct in_addr address_struct;
    address_struct.s_addr = address;
    return inet_ntoa(address_struct);
}

void config_host_net(Arguments *args) {
    if (args->net != "") {
        int pid = args->pid;
        string net_id = args->net_id;

        string cmd = "sudo ip link add name u-" + net_id +"-0 type veth peer name u-" + net_id + "-1";
        check_res(system(cmd.c_str()), "ip link add");

        cmd = "sudo ip link set u-" + net_id + "-1 netns " + to_string(pid);
        check_res(system(cmd.c_str()), "ip link set");

        cmd = "sudo ip link set u-" + net_id + "-0 up";
        check_res(system(cmd.c_str()), "ip link set");

        string next_ip = next_addr(args->net);
        cmd = "sudo ip addr add " + next_ip + "/24 dev u-" + net_id + "-0";
        check_res(system(cmd.c_str()), "ip set");
    }

}

void config_cont_net(Arguments *args) {
    if (args->net != "") {
        check_res(system("ip link set lo up"), "set lo");
        string com = "ip link set u-" + args->net_id + "-1 up";
        check_res(system(com.c_str()), "ip link up");

        string host_ip = args->net;
        com = string("ip addr add ") + host_ip + "/24 dev u-" + args->net_id + "-1";
        check_res(system(com.c_str()), "ip up");
    }
}

int container_main(void *arg) {
    Arguments *args = (Arguments *) arg;
    int err;

    char c;
    close(args->pipe_fd[1]);
    read(args->pipe_fd[0], &c, 1);

    check_res(mount("proc", (args->image_path + "/proc").c_str(), "proc", 0, NULL), "MOUNT PROC");
    check_res(mount("none", (args->image_path + "/tmp").c_str(), "tmpfs", 0, NULL), "MOUNT NONE");


    check_res(chdir(args->image_path.c_str()), "CHDIR");

    mknod("dev/null",    S_IFREG | 0666, 0);
    mknod("dev/zero",    S_IFREG | 0666, 0);
    mknod("dev/random",  S_IFREG | 0666, 0);
    mknod("dev/urandom", S_IFREG | 0666, 0);

    check_res(mount("/dev/null", "dev/null", NULL, MS_BIND, NULL), "null");
    check_res(mount("/dev/zero", "dev/zero", NULL, MS_BIND, NULL), "zero");
    check_res(mount("/dev/urandom", "dev/random", NULL, MS_BIND, NULL), "random");
    check_res(mount("/dev/urandom", "dev/urandom", NULL, MS_BIND, NULL), "urandom");

    mkdir("sys", 0755);
    check_res(mount("/sys", "sys", "/sys", MS_BIND | MS_REC | MS_RDONLY, NULL), "mount /sys");

    mkdir("dev/shm", 0755);
    check_res(mount("tmpfs", "dev/shm", "tmpfs", MS_NOSUID | MS_NODEV | MS_STRICTATIME, "mode=1777"),
                 "mount /dev/shm");

    string new_root_path = args->image_path;
    string old_root_path = args->image_path + "/old";
    mkdir("./old", 0777);

    check_res(mount(new_root_path.c_str(), new_root_path.c_str(), "bind", MS_BIND | MS_REC, NULL), "pivot");
    check_res(syscall(SYS_pivot_root, new_root_path.c_str(), old_root_path.c_str()), "pivot");

    check_res(chdir("/bin"), "chdir");

    check_res(umount2("/old", MNT_DETACH), "umount")

    setgroups(0, NULL);

    check_res(sethostname(CONT_HOSTNAME, strlen(CONT_HOSTNAME)), "sethostname");

    umask(0);

    check_res(setsid(), "setsid");

    config_cont_net(args);

    if (args->daemonize) {
        freopen("/dev/null", "r", stdin);
        freopen("/tmp/err", "w", stdout);
        freopen("/dev/null", "w", stderr);
        check_res(chdir("/"), "chdir");
    }

    execv(args->cmd.c_str(), &args->args[0]);
    exit(0);
}


int main(int argc, char *argv[]) {
    const int gid = getgid();
    const int uid = getuid();

    Arguments *args = new Arguments();
    args->net_id = "Net" + to_string(getpid());

    for (int i = 1; i < argc; ++i) {
        string cur = argv[i];
        if (cur == "-d") {
            args->daemonize = true;
        } else if (cur == "--cpu") {
            args->cpu_perc = atoi(argv[++i]);
        } else if (cur == "--net") {
            args->net = argv[++i];
        } else if (args->image_path.empty()) {
            args->image_path = cur;
        } else if (args->cmd.empty()) {
            args->cmd = cur;
            char * c = new char[cur.length()];
            strcpy(c, cur.c_str());
            args->args.push_back(c);
        } else {
            char * c = new char[cur.length()];
            strcpy(c, cur.c_str());
            args->args.push_back(c);
        }
    }
    int err;

    pipe(args->pipe_fd);

    int pid;

    int nses = CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD | CLONE_NEWUSER | CLONE_NEWNET;
    pid = clone(container_main, container_stack + STACK_SIZE, nses, args);
    args->pid = pid;

    check_res(pid, "CLONE");

    int mappid = pid;
    set_uid_map(mappid, 0, uid, 1);
    string cmd1 = "echo deny >> /proc/" + to_string(mappid) + "/setgroups";
    system(cmd1.c_str());
    set_gid_map(mappid, 0, gid, 1);

    const string cpu_cg_path = "/tmp/cgroup/cpu/" + to_string(pid);

    const string cmd = "mkdir -p " + cpu_cg_path;
    check_res(system(cmd.c_str()), "system call mkdir ");
    string chcmd = "chown -R " + std::to_string(getuid()) + ":" + std::to_string(getgid()) + " " + cpu_cg_path;
    check_res(system(chcmd.c_str()), "chown");

    const string set_period = "echo 1000000 >> " + cpu_cg_path + "/cpu.cfs_period_us";
    check_res(system(set_period.c_str()), "echo period");

    int quota = 1000000 * args->cpu_perc * thread::hardware_concurrency() / 100;
    const string set_quota = "echo " + to_string(quota) + " >> " + cpu_cg_path + "/cpu.cfs_quota_us";
    check_res(system(set_quota.c_str()), "echo quota");

    config_host_net(args);

    cout << pid << endl;
    ofstream f(CONT_LIST_FILE, ofstream::app | ofstream::out);
    f << pid << endl;
    f.close();

    close(args->pipe_fd[1]);

    if (!args->daemonize) {
        waitpid(pid, NULL, 0);
        remove_cont(pid);
    }
    delete args;
    _exit(0);
    return 0;
}