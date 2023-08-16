#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define SERVER_PORT 56789
#define LISTEN_BACKLOG 5
#define MAX_EVENT_NUM 1024

void sys_err(const char *err) {
  perror(err);
  exit(-1);
}

int main() {
  int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) sys_err("socket");

  int reuse = 1;
  int ret =
      setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  if (ret < 0) sys_err("setsockopt");

  struct sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERVER_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  ret = bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret < 0) sys_err("bind");

  ret = listen(listen_fd, LISTEN_BACKLOG);
  if (ret < 0) sys_err("listen");

  int epfd = epoll_create(MAX_EVENT_NUM);
  if (epfd < 0) sys_err("epoll_create");

  // 监听 listen_fd 的可读事件
  struct epoll_event event_tmp;
  event_tmp.events = EPOLLIN;
  event_tmp.data.fd = listen_fd;
  ret = epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &event_tmp);
  if (ret < 0) sys_err("epoll_ctl");

  for (;;) {
    static struct epoll_event events[MAX_EVENT_NUM];
    int ready_cnt = epoll_wait(epfd, events, MAX_EVENT_NUM, -1);
    if (ready_cnt < 0) sys_err("epoll_wait");
    if (ready_cnt == 0) continue;

    static char buf[BUFSIZ];
    for (int i = 0; i < ready_cnt; ++i) {  // 遍历所有就绪的文件描述符
      int fd_tmp = events[i].data.fd;
      if (fd_tmp == listen_fd) {  // 新连接
        static int conn_fd;
        static struct sockaddr_in client_addr;
        static socklen_t client_addr_len = sizeof(client_addr);

        conn_fd =
            accept(fd_tmp, (struct sockaddr *)&client_addr, &client_addr_len);
        if (conn_fd < 0) sys_err("accept");

        printf(
            "client: %s:%d\n",
            inet_ntop(AF_INET, &(client_addr.sin_addr), buf, INET_ADDRSTRLEN),
            ntohs(client_addr.sin_port));

        // 将其添加到内核事件表
        event_tmp.data.fd = conn_fd;
        event_tmp.events = EPOLLIN;
        ret = epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &event_tmp);
        if (ret < 0) sys_err("epoll_ctl");

      } else {  // 旧连接
        int num = recv(fd_tmp, buf, sizeof(buf), 0);
        // 收发数据
        if (num > 0) {
          for (int i = 0; i < num; ++i) {
            if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 'a' - 'A';
          }
          if (send(fd_tmp, buf, num, 0) == -1) sys_err("send");
          continue;
        }

        // 其他情况关闭连接
        ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fd_tmp, NULL);
        if (ret < 0) sys_err("epoll_ctl");
        close(fd_tmp);

        if (num == 0) {  // 客户端主动关闭
          printf("close connection\n");
        } else {  // recv 出错的情况
          perror("recv");
        }
      }
    }
  }

  close(epfd);
  close(listen_fd);
  return 0;
}
