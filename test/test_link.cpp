#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <string>
#include "head.h"

int main() {
  FormatFileSystem(root_path);
  LogIn("root", "root");
  need_log = false;
  CreateFile("a");

  for (int i = 0; i < MAX_BLOCK_NUMBER; ++i) {
    Link("a", std::to_string(i).c_str());
    DeleteFile(std::to_string(i).c_str());
  }

  CloseFileSystem();
  return 0;
}