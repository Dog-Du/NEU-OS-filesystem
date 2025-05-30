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
char *memory = nullptr;
bool need_log = true;

bool CloseFileSystem() {
  assert(pwrite(fd, memory, DISK_SIZE, 0) == DISK_SIZE);
  close(fd);
  return true;
}

// 格式化
bool FormatFileSystem(const char *file_name) {
  fd = open(file_name, O_CREAT | O_RDWR | O_TRUNC, 0b111111111);

  if (fd < 0) {
    fprintf(stderr, "格式化文件系统失败。");
    return false;
  }

  ftruncate(fd, DISK_SIZE);
  memory = (char *)mmap(NULL, DISK_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
  // 共享内存。
  if (memory == MAP_FAILED) {
    fprintf(stderr, "格式化文件系统失败。");
    return false;
  }

  memset(memory, 0, DISK_SIZE);
  superBlock *super = GetSuperBlock();
  super->stack_num = 1;
  super->stack[0] = 0;

  // 把所有块进行初始化
  for (int i = MAX_BLOCK_NUMBER - 1; i >= 1; --i) {
    ReleaseDataBlock(i);
  }

  // 根目录设置为空，谁都可以进行创建和删除文件。
  super->root_dir_id = NewFile(DIR_TYPE, "/", "");
  super->user_info_id = NewFile(USER_TYPE, "user_info", "root");

  inode *root_dir = GetInode(super->root_dir_id);
  root_dir->last_dir = -1;
  root_dir->link_cnt = -1;

  inode *user_info = GetInode(super->user_info_id);
  user_info->link_cnt = -1;
  user_info->last_dir = -1;

  // user_info_id应该永远都在open_file中
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

  // 共享内存
  memory = (char *)mmap(NULL, s.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (memory == MAP_FAILED) {
    fprintf(stderr, "文件系统打开失败。");
    return false;
  }

  // 读取
  assert(pread(fd, memory, DISK_SIZE, 0) == DISK_SIZE);
  return true;
}

/*----------------------几个指针强转型实现--------------------------------------------------*/
superBlock *GetSuperBlock() { return (superBlock *)memory; }

inode *GetInode(int index) { return (inode *)(memory + INODE_OFFSET + INODE_SIZE * index); }

dataBlock *GetBlock(int index) {
  return (dataBlock *)(memory + DATA_BLOCK_OFFSET + BLOCK_SIZE * index);
}

indexBlock *GetIndexBlock(int index) { return (indexBlock *)GetBlock(index); }

void PutBlock(int index, bool write) {
  if (write) {
    LOG("刷新块[%d]\n", index);
    dataBlock *block = GetBlock(index);
    assert(pwrite(fd, block, BLOCK_SIZE, DATA_BLOCK_OFFSET + BLOCK_SIZE * index) == BLOCK_SIZE);
  }
}

void PutInode(int index, bool write) {
  if (write) {
    LOG("刷新inode[%d]\n", index);
    inode *n = GetInode(index);
    assert(pwrite(fd, n, INODE_SIZE, INODE_OFFSET + INODE_SIZE * index) == INODE_SIZE);
  }
}

void PutSuperBlock(bool write) {
  if (write) {
    assert(pwrite(fd, GetSuperBlock(), BLOCK_SIZE, 0) == BLOCK_SIZE);
  }
}
/*----------------------几个指针强转型实现--------------------------------------------------*/

/*----------------------对超级块进行操作实现分配释放-----------------------------------------*/
int AllocDataBlock() {
  superBlock *super = GetSuperBlock();
  int ret = 0;
  constexpr int max_length = (sizeof(super->stack) / sizeof(int)) - 1;

  // 超级栈不足
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

    // 把信息拷贝到超级栈中。
    if (super->stack[0] >= MAX_BLOCK_NUMBER) {
      super->stack_num = 0;
    }
  }

  // 超级栈充足时，从超级栈中分配
  if (super->stack_num > 1 || (super->stack[0] > 0 && super->stack[0] < MAX_BLOCK_NUMBER)) {
    ret = super->stack[--super->stack_num];
    LOG("分配块%d\n", ret);
  } else {
    ret = 0;
    LOG("块不足\n");
  }

  PutSuperBlock(true);

  // 清空
  if (ret > 0) {
    memset(GetInode(ret), 0, INODE_SIZE);
    memset(GetBlock(ret), 0, BLOCK_SIZE);
    PutInode(ret, true);
    PutBlock(ret, true);
  }
  return ret >= MAX_BLOCK_NUMBER || ret <= 0 ? 0 : ret;
}

void ReleaseDataBlock(int index) {
  superBlock *super = GetSuperBlock();
  constexpr int max_length = (sizeof(super->stack) / sizeof(int)) - 1;
  super->stack[super->stack_num++] = index;  // 追加到尾部

  // 如果太大, 分组分块, 注意到, 分配之后的块的组长块就是最后释放的块.
  // 所以，如果超级栈如果不足，需要拉取组长的块的时候，第一个被分配的就是组长块
  while (super->stack_num >= max_length) {
    memcpy(GetBlock(index), super, BLOCK_SIZE);  // 既是组长块也是空闲块。
    super->stack[0] = index;                     // 指向下一个组长块
    super->stack_num = 1;
    PutBlock(index, true);
  }

  PutSuperBlock(true);
  LOG("释放块%d\n", index);
}
/*----------------------对超级块进行操作实现分配释放-----------------------------------------*/

// void FlushDisk() { assert(pwrite(fd, memory, DISK_SIZE, 0) == DISK_SIZE); }