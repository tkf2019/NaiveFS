#include <sys/mman.h>

#include <cstdio>
using namespace std;

struct IObuf {
  char ind[200000010], *i, *o;
  IObuf() : o(ind) {}
  ~IObuf() { fwrite(ind, 1, o - ind, stdout); }
  inline operator unsigned int() {
    unsigned int ret = 0;
    while (*i < 48)
      if (*i++ == 45)
        ;
    while (*i > 32) ret = ret * 10 + *i++ - 48;
    return ret;
  }
  inline void write(int x) {
    static char c[12];
    char *t = c;
    if (!x)
      *o++ = 48;
    else {
      if (x < 0) *o++ = '-', x = -x;
      while (x) {
        int y = x / 10;
        *t++ = x - y * 10 + '0', x = y;
      }
      while (t != c) *o++ = *--t;
    }
    *o++ = '\n';
  }
} io;

int main() {
  unsigned int n, x, a, b, c;
  scanf("%d%d%d%d%d", &n, &x, &a, &b, &c);
  while (n--) io.write((int)(x = x * x * a + x * b + c));
  return 0;
}