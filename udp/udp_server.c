#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));

  char buf[BUFFER_SIZE + 1];
  struct sockaddr_in client_addr;
  socklen_t client_addr_len;
  for (;;) {
    client_addr_len = sizeof(client_addr);
    int recv_num = recvfrom(sockfd, buf, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len);
    if (recv_num < 0) sys_err("recvfrom");
    buf[recv_num] = '\0';

    static char ip_str[INET_ADDRSTRLEN];
    printf("received %d bytes from %s:%d\n", recv_num,
           inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN), ntohs(client_addr.sin_port));
    printf("%s\n", buf);

    for (int i = 0; i < recv_num; ++i) {
      if (buf[i] >= 'a' && buf[i] <= 'z') buf[i] -= 'a' - 'A';
    }

    int send_num = sendto(sockfd, buf, recv_num, 0, (struct sockaddr *)&client_addr, client_addr_len);
    if (send_num < 0) sys_err("sendto");
  }

  close(sockfd);

  return 0;
}
