#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cassert>
#include <string>
#include "head.h"

char buf[MAX_FILE_SIZE];

void create_maxfile(const char *file) {
  CreateFile(file);

  assert(Append(Open(file), MAX_FILE_SIZE, buf) == MAX_FILE_SIZE);

  ReadFile(file);
}

int main() {
  for (int i = 0; i < MAX_FILE_SIZE; ++i) {
    buf[i] = 'a';
  }

  FormatFileSystem(root_path);
  LogIn("root", "root");
  need_log = false;

  for (int i = 0; i <= 100; ++i) {
    std::string s = std::to_string(i);
    create_maxfile(s.c_str());
    printf("文件%d完成\n", i);
  }

  CloseFileSystem();
  return 0;
}