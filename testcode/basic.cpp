#include <bits/stdc++.h>
#include <fcntl.h>
#include <unistd.h>
int main() {
  int fd = open("test/a", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) printf("[%d,err=%d]", fd, errno);
  char rdbuf[10000];
  int bit = read(fd, rdbuf, 10000);
  for (int i = 0; i < bit; ++i) putchar(rdbuf[i]);
  char buf[] = "hello world!";
  int r = write(fd, buf, sizeof(buf));
  assert(r == sizeof(buf));
  printf("[bytes=%d,real=%d,err=%d]", sizeof(buf), r, errno);
  return 0;
}
