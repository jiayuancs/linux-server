#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 56789
#define BUFFER_SIZE 1024

void sys_err(const char *err) {
  perror(err);
  exit(-1);
}

int main() {
  int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) sys_err("socket");

  struct sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERVER_PORT);
  inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);
  char buf[BUFFER_SIZE + 1];

  printf("> ");
  while (std::cin.getline(buf, BUFFER_SIZE)) {
    if (buf[0] == '\0') {
      printf("> ");
      continue;
    }

    if (strcmp(buf, "exit") == 0) break;

    int send_num = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *)&addr, sizeof(addr));
    if (send_num <= 0) sys_err("sendto");

    int recv_num = recvfrom(sockfd, buf, BUFFER_SIZE, 0, NULL, NULL);
    if (recv_num <= 0) sys_err("recvfrom");
    buf[recv_num] = '\0';
    printf("%s\n> ", buf);
  }

  close(sockfd);

  return 0;
}
