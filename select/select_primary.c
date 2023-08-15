#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT 56789
#define LISTEN_BACKLOG 5

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

  fd_set rset, allset;
  FD_ZERO(&allset);
  FD_SET(listen_fd, &allset);
  int max_fd = listen_fd;

  for (;;) {
    rset = allset;
    int ready_cnt = select(max_fd + 1, &rset, NULL, NULL, NULL);
    if (ready_cnt < 0) sys_err("select");

    static char buf[BUFSIZ];

    // 有新的连接请求
    if (FD_ISSET(listen_fd, &rset)) {
      static int conn_fd;
      static struct sockaddr_in client_addr;
      static socklen_t client_addr_len = sizeof(client_addr);

      // accept 函数不会阻塞
      conn_fd =
          accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_len);
      if (conn_fd < 0) sys_err("accept");
      if (conn_fd == 0) continue;

      printf("client: %s:%d\n",
             inet_ntop(AF_INET, &(client_addr.sin_addr), buf, INET_ADDRSTRLEN),
             ntohs(client_addr.sin_port));

      FD_SET(conn_fd, &allset);  // 将新的连接描述符添加到 allset 集合中

      if (conn_fd > max_fd) max_fd = conn_fd;

      if (ready_cnt == 1)
        continue;  // 如果只有 listen_fd 有事件，则后续的 for 无需执行
    }

    // 遍历所有的连接描述符，处理有数据到达的连接
    for (int i = listen_fd + 1; i <= max_fd; ++i) {  // 效率低
      if (!FD_ISSET(i, &rset)) continue;

      // 读写数据
      int num = recv(i, buf, sizeof(buf), 0);
      if (num == 0) {  // 客户端主动断开连接
        printf("close connection\n");
        FD_CLR(i, &allset);
        close(i);
      } else if (num > 0) {  // 读取客户端发送的数据
        for (int i = 0; i < num; ++i) {
          if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 'a' - 'A';
        }
        if (send(i, buf, num, 0) == -1) sys_err("send");
      } else {
        sys_err("recv");
      }
    }
  }

  close(listen_fd);
  return 0;
}
