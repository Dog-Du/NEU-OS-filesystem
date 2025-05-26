#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "head.h"
#include "print.h"

void ReadFile(const char *file) {
  int fd = Open(file);
  if (fd < 0) {
    fprintf(stderr, "读取出错\n");
    return;
  }

  if (IsOpen(fd) == false) {
    fprintf(stderr, "未打开\n");
    return;
  }

  inode *n = GetInode(fd);
  if (n->type == LINK_TYPE) {
    n = GetInode(n->link_inode);
  }

  char *s = (char *)alloca(n->length + 10);
  memset(s, 0, n->length + 10);

  int len = Read(fd, 0, n->length, s);
  PRINT_FONT_GRE
  fprintf(stdout, "%s\n", s);
  PRINT_FONT_RED
  fprintf(stdout, "共读取%d字节\n", len);
}