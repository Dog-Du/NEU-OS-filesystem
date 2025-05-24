#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#include "head.h"

int fd = -1;
char memory[DISK_SIZE];

bool CloseFileSystem() {
  assert(pwrite(fd, memory, DISK_SIZE, 0) == DISK_SIZE);
  close(fd);
  return true;
}

bool FormatFileSystem(const char *file_name) {
  struct stat s;
  fd = open(file_name, O_CREAT | O_RDWR | O_TRUNC, 0b111111111);

  if (fd < 0) {
    fprintf(stderr, "格式化文件系统失败。");
    return false;
  }

  ftruncate(fd, DISK_SIZE);
  if (memory == MAP_FAILED) {
    fprintf(stderr, "格式化文件系统失败。");
    return false;
  }

  memset(memory, 0, DISK_SIZE);
  superBlock *super = GetSuperBlock();
  super->stack_num = 1;
  super->stack[0] = 0;

  for (int i = 0; i < MAX_BLOCK_NUMBER; ++i) {
    ReleaseDataBlock(i);
  }

  super->root_dir_id = AllocDataBlock();
  super->user_info_id = AllocDataBlock();

  inode *root_dir = GetInode(super->root_dir_id);
  root_dir->type = DIR_TYPE;
  root_dir->last_dir = -1;
  root_dir->link_cnt = -1;
  root_dir->length = 0;

  root_dir->id = super->root_dir_id;
  memcpy(root_dir->file_name, "/", sizeof("/"));
  memcpy(root_dir->owner_name, "root", sizeof("root"));

  inode *user_info = GetInode(super->user_info_id);
  user_info->type = USER_TYPE;
  user_info->id = super->user_info_id;
  user_info->link_cnt = 0;
  user_info->length = 0;
  user_info->last_dir = 0;

  memcpy(user_info->owner_name, "root", sizeof("root"));
  memcpy(user_info->file_name, "user_info", sizeof("user_info"));

  open_file.insert(super->user_info_id);
  UserAdd("root", "root", "root");
  assert(pwrite(fd, memory, DISK_SIZE, 0) == DISK_SIZE);
  return true;
}

bool OpenFileSystem(const char *file_name) {
  fd = open(file_name, O_CREAT | O_RDWR, 0b111111111);

  if (fd < 0) {
    fprintf(stderr, "文件系统打开失败。");
    return false;
  }

  struct stat s;
  fstat(fd, &s);
  if (s.st_size != DISK_SIZE) {
    close(fd);
    return FormatFileSystem(file_name);
  }

  assert(pread(fd, memory, DISK_SIZE, 0) == DISK_SIZE);
  return true;
}

superBlock *GetSuperBlock() { return (superBlock *)memory; }

inode *GetInode(int index) { return (inode *)(memory + INODE_OFFSET + INODE_SIZE * index); }

dataBlock *GetBlock(int index) {
  return (dataBlock *)(memory + DATA_BLOCK_OFFSET + BLOCK_SIZE * index);
}

indexBlock *GetIndexBlock(int index) { return (indexBlock *)GetBlock(index); }

void PutBlock(int index, bool write) {
  if (write) {
    dataBlock *block = GetBlock(index);
    assert(pwrite(fd, block, BLOCK_SIZE, DATA_BLOCK_OFFSET + BLOCK_SIZE * index) == BLOCK_SIZE);
  }
}

void PutInode(int index, bool write) {
  if (write) {
    inode *n = GetInode(index);
    assert(pwrite(fd, n, INODE_SIZE, INODE_OFFSET + INODE_SIZE * index) == INODE_SIZE);
  }
}

int AllocDataBlock() {
  superBlock *super = GetSuperBlock();
  int ret;
  constexpr int max_length = (sizeof(super->stack) / sizeof(int)) - 1;

  while (super->stack_num <= 1 && super->stack[0] != 0) {
    int block = super->stack[0];

    if (block <= 0 || block >= MAX_BLOCK_NUMBER) {
      break;
    }

    int root = super->root_dir_id;
    int user = super->user_info_id;
    memcpy(super, GetBlock(block), BLOCK_SIZE);

    super->stack_num = max_length;
    super->root_dir_id = root;
    super->user_info_id = user;

    if (super->stack[0] >= MAX_BLOCK_NUMBER) {
      super->stack_num = 0;
    }
  }

  if (super->stack_num > 1 || (super->stack[0] > 0 && super->stack[0] < MAX_BLOCK_NUMBER)) {
    ret = super->stack[--super->stack_num];
    PutBlock(0, true);
    fprintf(stderr, "分配块%d\n", ret);
  } else {
    ret = 0;
    fprintf(stderr, "块不足\n");
  }

  return ret >= MAX_BLOCK_NUMBER ? 0 : ret;
}

void ReleaseDataBlock(int index) {
  superBlock *super = GetSuperBlock();
  constexpr int max_length = (sizeof(super->stack) / sizeof(int)) - 1;
  super->stack[super->stack_num++] = index;

  while (super->stack_num >= max_length) {
    memcpy(GetBlock(index), super, BLOCK_SIZE);
    super->stack[0] = index;
    super->stack_num = 1;
    PutInode(index, true);
  }

  PutBlock(0, true);
  fprintf(stderr, "释放块%d\n", index);
}