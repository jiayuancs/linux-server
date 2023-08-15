#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define SERVER_PORT 56789
#define LISTEN_BACKLOG 5

void sys_err(const char *err);
void wait_child(int signum);
void process_connect(int fd);

int main() {
  // 阻塞 SIGCHLD 信号
  sigset_t set, oldset;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  sigprocmask(SIG_BLOCK, &set, &oldset);

  int lfd = socket(PF_INET, SOCK_STREAM, 0);
  if (lfd == -1) sys_err("socket");

  // 强制重用处于 TIME_WAIT 状态的 socket
  int reuse = 1;
  int ret = setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  if (ret == -1) sys_err("setsockopt");

  struct sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERVER_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  ret = bind(lfd, (struct sockaddr *)&addr, sizeof(addr));

  ret = listen(lfd, LISTEN_BACKLOG);
  if (ret == -1) sys_err("listen");

  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  for (;;) {
    int cfd = accept(lfd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (cfd == -1) sys_err("accept");

    pid_t pid = fork();
    if (pid == 0) {  // 子进程处理连接
      close(lfd);

      char client_ip_str[INET_ADDRSTRLEN];
      printf("client: %s:%d\n",
             inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip_str,
                       INET_ADDRSTRLEN),
             ntohs(client_addr.sin_port));
      process_connect(cfd);
      printf("close connection\n");
      close(cfd);
      return 0;
    } else if (pid > 0) {
      close(cfd);

      // 注册 SIGCHLD 信号处理函数(仅执行一次)
      static int sig_flag = 0;
      if (sig_flag == 0) {
        struct sigaction act;
        act.sa_handler = wait_child;
        act.sa_flags = SA_RESTART;  // 自动重新执行被中断的系统调用
        sigemptyset(&act.sa_mask);
        if (sigaction(SIGCHLD, &act, NULL) == -1) sys_err("sigaction");
        sigprocmask(SIG_SETMASK, &oldset, NULL);
        sig_flag = 1;
      }
      continue;
    } else {
      sys_err("fork");
    }
  }

  close(lfd);

  return 0;
}

void sys_err(const char *err) {
  perror(err);
  exit(-1);
}

// SGICHLD 信号处理函数，回收子进程
void wait_child(int signum) {
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    ;
}

// 处理一个连接
void process_connect(int fd) {
  static char buf[BUFSIZ] = {0};
  int num = 0;
  while ((num = recv(fd, &buf, sizeof(buf), 0)) > 0) {
    buf[num] = '\0';
    for (int i = 0; i < num; ++i) {
      if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 'a' - 'A';
    }
    if (send(fd, buf, num, 0) == -1) sys_err("send");
  }
}
