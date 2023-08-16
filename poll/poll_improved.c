#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT 56789
#define LISTEN_BACKLOG 5
#define MAX_FD_NUM 1024

void sys_err(const char *err);
void swap_pollfd(struct pollfd *lhs, struct pollfd *rhs);

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

  int max_idx = 0;
  static struct pollfd pfds[MAX_FD_NUM];
  pfds[max_idx].fd = listen_fd;
  pfds[max_idx].events = POLLIN;
  pfds[max_idx].revents = 0;
  for (int i = max_idx + 1; i < MAX_FD_NUM; ++i) {
    pfds[i].fd = -1;
  }

  for (;;) {
    int ready_cnt = poll(pfds, max_idx + 1, -1);
    if (ready_cnt < 0) sys_err("poll");
    if (ready_cnt == 0) continue;

    static char buf[BUFSIZ];

    // 新连接
    if (pfds[0].revents & POLLIN) {
      static int conn_fd;
      static struct sockaddr_in client_addr;
      static socklen_t client_addr_len = sizeof(client_addr);

      conn_fd =
          accept(pfds[0].fd, (struct sockaddr *)&client_addr, &client_addr_len);
      if (conn_fd < 0) sys_err("accept");

      printf("client: %s:%d\n",
             inet_ntop(AF_INET, &(client_addr.sin_addr), buf, INET_ADDRSTRLEN),
             ntohs(client_addr.sin_port));

      // 将 conn_fd 添加到数组中的第一个空闲位置中
      ++max_idx;
      pfds[max_idx].fd = conn_fd;
      pfds[max_idx].events = POLLIN;
      pfds[max_idx].revents = 0;

      if (--ready_cnt == 0) continue;
    }

    // 遍历所有的连接描述符，处理有数据到达的连接
    for (int i = 1; i <= max_idx; ++i) {
      if (!(pfds[i].revents & POLLIN)) continue;

      // 读写数据
      int num = recv(pfds[i].fd, buf, sizeof(buf), 0);
      if (num == 0) {  // 客户端主动断开
        close(pfds[i].fd);
        printf("close connection\n");

        // 将其与数组最后一个有效元素交换，然后删除最后一个有效元素
        pfds[i].fd = -1;
        swap_pollfd(&pfds[i], &pfds[max_idx]);
        --max_idx;
      } else if (num > 0) {  // 读取客户端发送的数据
        for (int i = 0; i < num; ++i) {
          if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 'a' - 'A';
        }
        if (send(pfds[i].fd, buf, num, 0) == -1) sys_err("send");
      } else {
        sys_err("recv");
      }

      if (--ready_cnt <= 0) break;
    }
  }

  close(listen_fd);
  return 0;
}

void sys_err(const char *err) {
  perror(err);
  exit(-1);
}

// 交换两个 struct pollfd 类型变量的值
void swap_pollfd(struct pollfd *lhs, struct pollfd *rhs) {
  static struct pollfd tmp;
  tmp.fd = lhs->fd;
  tmp.events = lhs->events;
  tmp.revents = lhs->revents;

  lhs->fd = rhs->fd;
  lhs->events = rhs->events;
  lhs->revents = rhs->revents;

  rhs->fd = tmp.fd;
  rhs->events = tmp.events;
  rhs->revents = tmp.revents;
}
