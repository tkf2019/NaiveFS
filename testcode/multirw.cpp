#include <bits/stdc++.h>
#include <fcntl.h>
#include <unistd.h>
using namespace std;
char aa[3000000];
int test(int Z) {
  srand(time(0));
  char a[30000], b[30000];
  int my = Z * sizeof(a);
  int fd = open("test/cc", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd <= 0) printf("err[%d,%d]", fd, errno), fflush(stdout);
  for (int j = 0; j < sizeof(a); ++j) a[j] = rand() & 255;
  int r = pwrite(fd, a, sizeof(a), my);
  if (r < 0) printf("err[%d,%d]", r, errno), fflush(stdout);
  assert(r == sizeof(a));
  r = pread(fd, b, sizeof(a), my);
  if (r < 0) printf("err[%d,%d]", r, errno), fflush(stdout);
  assert(r == sizeof(a));
  assert(memcmp(a, b, sizeof(a)) == 0);
  int Q = 10;
  for (int i = 1; i <= Q; ++i) {
    int L = abs(rand()) % sizeof(a);
    int R = abs(rand()) % sizeof(a);
    if (L > R) swap(L, R);
    for (int j = L; j < R; ++j) a[j] = rand() & 255;
    int r = pwrite(fd, a + L, R - L + 1, L + my);
    if (r < 0) printf("err[%d,%d]", r, errno), fflush(stdout);
    assert(r == R - L + 1);
    L = abs(rand()) % sizeof(a);
    R = abs(rand()) % sizeof(a);
    if (L > R) swap(L, R);
    r = pread(fd, b, R - L + 1, L + my);
    if (r < 0) printf("err[%d,%d]", r, errno), fflush(stdout);
    assert(r == R - L + 1);
    assert(memcmp(b, a + L, R - L + 1) == 0);
  }
}

int main() {
  int fd = open("test/cc", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd <= 0) printf("err[%d,%d]", fd, errno), fflush(stdout);
  for (int j = 0; j < sizeof(aa); ++j) aa[j] = rand() & 255;
  int r = pwrite(fd, aa, sizeof(aa), 0);
  if (r < 0) printf("err[%d,%d]", r, errno), fflush(stdout);
  vector<thread> v;
  for (int i = 0; i < 10; ++i) v.emplace_back([i]() { test(i); });
  for (auto& x : v) x.join();
}