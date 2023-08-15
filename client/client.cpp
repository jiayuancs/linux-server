#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

#define SERVER_PORT 56789
#define SERVER_IP "127.0.0.1"

void sys_err(const char *info) {
  perror(info);
  exit(-1);
}

int main(int argc, char *argv[]) {
  // 1. socket
  int cfd = socket(PF_INET, SOCK_STREAM, 0);
  if (cfd == -1) sys_err("socket");

  // 2. connect
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
  int ret = connect(cfd, (sockaddr *)&server_addr, sizeof(server_addr));
  if (ret == -1) sys_err("connect");
  printf("connected\n");

  // 3. recv and send
  char buf[BUFSIZ] = {0};
  int num = 0;
  printf("> ");
  while (std::cin.getline(buf, sizeof(buf))) {
    if ((num = strlen(buf)) == 0) {
      printf("> ");
      continue;
    }
    if (strcmp(buf, "exit") == 0) break;  // 输入 "exit" 退出客户端
    if (send(cfd, buf, num, 0) == -1) sys_err("send");
    if ((num = recv(cfd, buf, sizeof(buf), 0)) == -1) sys_err("recv");
    buf[num] = '\0';
    printf("recv %d bytes: %s\n> ", num, buf);
  }

  // 4. shutdown
  shutdown(cfd, SHUT_RDWR);
  printf("close socket\n");

  return 0;
}
