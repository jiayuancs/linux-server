#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#define SERVER_PORT 56789
#define LISTEN_BACKLOG 5

#define MAX_EVENT_NUM 1024
#define BUFFER_SIZE 10

void sys_err(const char *err) {
  perror(err);
  exit(-1);
}

// 设置非阻塞
int setnonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0) return flags;
  flags |= O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags);
}

// 监听 fd 的可读事件，enable_et 表示是否使用 ET 模式
int addfd(int epollfd, int fd, int enable_et) {
  static struct epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN;
  if (enable_et) event.events |= EPOLLET;
  return epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
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

  // 监听 listen_fd 的可读事件(使用 ET 模式)
  ret = addfd(epfd, listen_fd, 1);
  if (ret < 0) sys_err("addfd");
  // 设置非阻塞
  ret = setnonblocking(listen_fd);
  if (ret < 0) sys_err("setnonblocking");

  for (;;) {
    static struct epoll_event events[MAX_EVENT_NUM];
    int ready_cnt = epoll_wait(epfd, events, MAX_EVENT_NUM, -1);
    if (ready_cnt < 0) sys_err("epoll_wait");

    static char buf[BUFFER_SIZE];
    for (int i = 0; i < ready_cnt; ++i) {  // 遍历所有就绪的文件描述符
      int fd_tmp = events[i].data.fd;
      if (fd_tmp == listen_fd) {  // 新连接
        static int conn_fd;
        static struct sockaddr_in client_addr;
        static socklen_t client_addr_len = sizeof(client_addr);
        static char ip_str[INET_ADDRSTRLEN];

        conn_fd =
            accept(fd_tmp, (struct sockaddr *)&client_addr, &client_addr_len);
        if (conn_fd < 0) sys_err("accept");

        printf("client: %s:%d\n",
               inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str,
                         INET_ADDRSTRLEN),
               ntohs(client_addr.sin_port));

        // 监听 conn_fd 的可读事件(使用 ET 模式)
        ret = addfd(epfd, conn_fd, 1);
        if (ret < 0) sys_err("addfd");
        // 设置非阻塞
        ret = setnonblocking(conn_fd);
        if (ret < 0) sys_err("setnonblocking");

      } else {  // 旧连接
        for (int recv_times = 1;; ++recv_times) {
          printf("recv %d\n", recv_times);
          int recv_num = recv(fd_tmp, buf, sizeof(buf), 0);  // 非阻塞
          if (recv_num < 0) {
            // 数据已读完，则跳出循环
            if (errno == EAGAIN) break;
            // 其他错误则关闭连接
            ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fd_tmp, NULL);
            if (ret < 0) sys_err("epoll_ctl");
            close(fd_tmp);
            perror("recv");
            break;
          } else if (recv_num == 0) {  // 客户端主动关闭
            ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fd_tmp, NULL);
            if (ret < 0) sys_err("epoll_ctl");
            close(fd_tmp);
            printf("close connection\n");
            break;
          } else if (recv_num > 0) {  // 收发数据
            for (int i = 0; i < recv_num; ++i) {
              if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 'a' - 'A';
            }
            if (send(fd_tmp, buf, recv_num, 0) == -1) sys_err("send");
            if (recv_num < sizeof(buf))
              break;  // 如果没有读满缓冲区，则无需再读下一次
          }
        }
      }
    }
  }

  close(epfd);
  close(listen_fd);
  return 0;
}
