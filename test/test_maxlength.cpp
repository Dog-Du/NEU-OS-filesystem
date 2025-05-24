#include "head.h"
#include <stdio.h>

int main() {
  FormatFileSystem(root_path);
  LogIn("root", "root");
  CreateFile("a");

  int len = 0;
  while (1) {
    Append(Open("a"), 1, "a");
    ++len;
    printf("%d\n", len);

    if (len > MAX_FILE_SIZE + 10) {
      break;
    }
  }

  ReadFile("a");
  return 0;
}