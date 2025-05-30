#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#include "head.h"

bool Load(const char *src, const char *file) {
  // 检查
  int fd = open(src, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "不存在文件%s\n", src);
    close(fd);
    return false;
  }

  int index = Open(file);
  if (index < 0 || GetInode(index)->type != FILE_TYPE) {
    fprintf(stderr, "不存在文件%s\n", file);
    close(fd);
    return false;
  }

  if (ValidateCurrent(index) == false) {
    fprintf(stderr, "无权限\n");
    close(fd);
    return false;
  }

  if (IsOpen(index) == false) {
    fprintf(stderr, "未打开\n");
    close(fd);
    return false;
  }

  // 先拷贝到buf，再append到文件
  struct stat s;
  fstat(fd, &s);
  int len = s.st_size;
  char *buf = (char *)alloca(len + 1);
  memset(buf, 0, len + 1);
  read(fd, buf, len);

  Append(index, len, buf);
  close(fd);
  return true;
}