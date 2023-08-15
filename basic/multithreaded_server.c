#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT 56789
#define LISTEN_BACKLOG 5

// 传给子线程的信息
struct s_info {
  struct sockaddr_in addr;
  socklen_t addr_len;
  int fd;
};

void sys_err(const char *err);
void *process_connect(void *arg);

int main() {
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
  if (ret == -1) sys_err("bind");

  ret = listen(lfd, LISTEN_BACKLOG);
  if (ret == -1) sys_err("listen");

  for (;;) {
    // 在堆上申请一块空间存放socket信息，以便传给子线程
    struct s_info *info = (struct s_info *)malloc(sizeof(struct s_info));
    if (info == NULL) sys_err("malloc");

    info->addr_len = sizeof(info->addr);  // 勿忘
    info->fd = accept(lfd, (struct sockaddr *)&(info->addr), &(info->addr_len));
    if (info->fd == -1) sys_err("accept");

    // 创建子线程
    pthread_t tid;
    errno = pthread_create(&tid, NULL, process_connect, (void *)info);
    if (errno == -1) sys_err("pthread_create");

    // 分离子线程
    errno = pthread_detach(tid);
    if (errno == -1) sys_err("pthread_detach");
  }

  return 0;
}

void sys_err(const char *err) {
  perror(err);
  exit(-1);
}

// 处理一个连接
void *process_connect(void *arg) {
  struct s_info *info = (struct s_info *)arg;
  static char buf[BUFSIZ] = {0};

  printf("client: %s:%d\n",
         inet_ntop(AF_INET, &(info->addr.sin_addr), buf, INET_ADDRSTRLEN),
         ntohs(info->addr.sin_port));

  int num = 0;
  while ((num = recv(info->fd, &buf, sizeof(buf), 0)) > 0) {
    buf[num] = '\0';
    for (int i = 0; i < num; ++i) {
      if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 'a' - 'A';
    }
    if (send(info->fd, buf, num, 0) == -1) sys_err("send");
  }

  close(info->fd);
  free(info);
  printf("close connection\n");
  return NULL;
}
