#include <stdio.h>
#include "head.h"

int main() {
  FormatFileSystem(root_path);
  LogIn("root", "root");
  need_log = false;
  CreateFile("a");
  constexpr char buf[] = "abcdefghijklmnopqrstuvwxyz";
  int len = 0;
  while (1) {
    len += Append(Open("a"), sizeof(buf), buf);

    printf("%d\n", len);

    if (len >= MAX_FILE_SIZE) {
      break;
    }
  }

  ReadFile("a");
  return 0;
}