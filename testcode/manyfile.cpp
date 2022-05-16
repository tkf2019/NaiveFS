#include <bits/stdc++.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;
char a[100], b[100];
int main() {
  srand(time(0));
  for (int T = 0; T < 2000; ++T) {
    char name[100];
    sprintf(name, "test/dd%d", T);
    // printf("%d begin\n",T);fflush(stdout);
    int fd = open(name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd < 0) printf("open[%d,%d]", fd, errno), fflush(stdout);
    for (int j = 0; j < sizeof(a); ++j) a[j] = rand() & 255;
    int r = write(fd, a, sizeof(a));
    if (r != sizeof(a)) printf("[%d,%d]", r, errno), fflush(stdout);
    assert(r == sizeof(a));
    r = pread(fd, b, sizeof(a), 0);
    if (r != sizeof(a)) printf("[%d,%d]", r, errno), fflush(stdout);
    assert(r == sizeof(a));
    assert(memcmp(a, b, sizeof(a)) == 0);
    // printf("%d end\n",T);fflush(stdout);
    close(fd);
  }
}