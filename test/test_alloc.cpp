#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <string>
#include "head.h"

char buf[MAX_FILE_SIZE];

int main() {
  FormatFileSystem(root_path);
  LogIn("root", "root");

  for (int i = 0; i < MAX_BLOCK_NUMBER; ++i) {
    std::string s = std::to_string(i);
    CreateFile(s.c_str());
    std::cout << i << std::endl;
    assert(Append(Open(s.c_str()), MAX_FILE_SIZE, buf) == MAX_FILE_SIZE);
    ReadFile(s.c_str());
  }

  CloseFileSystem();
  return 0;
}