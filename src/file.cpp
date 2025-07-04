#include <string.h>
#include "head.h"

// 我觉得这个函数写的挺好，屏蔽了文件的多级索引，直接抽象成了一个块数组，通过下标来访问对应块
// 要考虑到，写入的时候可能会有空心，也就是后面的块分配了，但是中间的块却没有分配
// 不过只需要保证，分配来的块是clear的全零即可，因此需要在AllocaDataBlock中清空。
// 实现了文件是block块的抽象。
///@param n 传入文件的inode
///@param i 文件分为若干块，这里i是第i块的意思
///@param index 部分地方调用可能需要知道块在整个磁盘的位置
///@return 返回块
static dataBlock *getBlock(inode *n, int i, int *index) {
  if (i < MAX_FIRST_INDEX) {
    n->first_index[i] = (n->first_index[i] <= 0 ? AllocDataBlock() : n->first_index[i]);
    *index = n->first_index[i];
    return (n->first_index[i] <= 0 ? nullptr : GetBlock(n->first_index[i]));
  } else {
    // 目录类型，不分配第二个索引块。
    if (n->type == DIR_TYPE) {
      *index = 0;
      return nullptr;
    }

    if (n->second_index <= 0) {
      n->second_index = AllocDataBlock();
      if (n->second_index <= 0) {
        return nullptr;
      }

      // 清空磁盘块：因为分配来的块可能是脏数据块。当用它来当作二级索引块的时候，需要先清空.
      // 而非二级索引块在分配来的时候，不需要清空。因为不记录状态信息。
      // 但是二级索引块，用 0 表示空闲，这个状态信息。
      memset(GetBlock(n->second_index), 0, BLOCK_SIZE);
      PutBlock(n->second_index, true);
    }

    LOG("访问二级索引块%d\n", n->second_index);
    indexBlock *block = GetIndexBlock(n->second_index);
    i -= MAX_FIRST_INDEX;
    block->data_block[i] = (block->data_block[i] <= 0 ? AllocDataBlock() : block->data_block[i]);
    *index = block->data_block[i];
    return (block->data_block[i] <= 0 ? nullptr : GetBlock(block->data_block[i]));
  }
  return nullptr;
}

int NewFile(file_type type, const char *file_name, const char *owner_name) {
  const int file_name_len = strlen(file_name);
  const int owner_name_len = strlen(owner_name);
  if (file_name_len >= MAX_NAME_LENGTH || owner_name_len >= MAX_NAME_LENGTH) {
    fprintf(stderr, "文件名或所有者名过长\n");
    return -1;
  }

  int index = AllocDataBlock();
  if (index <= 0) {
    return index;
  }

  inode *n = GetInode(index);
  memset(n, 0, INODE_SIZE);
  n->type = type;
  n->link_cnt = 1;
  n->id = index;
  n->first_index[0] = index;
  n->length = 0;
  n->second_index = 0;
  // 因为inode节点和block节点共用编号，所以这个编号分配给inode之后，会让其对应的block节点浪费，为了减少浪费，把它分配给第一块
  // 不过对于链接来说，没啥用。

  memcpy(n->file_name, file_name, file_name_len);
  memcpy(n->owner_name, owner_name, owner_name_len);

  PutInode(index, true);
  return index;
}

// 在写入的时候，指定位置写入，可能会导致文件中间是空的。读取的时候要小心
// 实现了文件是字节数组的抽象。
int Write(int index, int pos, int len, const char *buf) {
  if (GetSuperBlock()->user_info_id != index && ValidateCurrent(index) == false) {
    fprintf(stderr, "无权限\n");
    return 0;
  }
  inode *n = GetInode(index);

  if (IsOpen(index) == 0 && n->type == FILE_TYPE) {
    fprintf(stderr, "未打开文件\n");
    return 0;
  }

  if (index <= 0) {
    return 0;
  }

  if (n->type == LINK_TYPE) {
    n = GetInode(n->link_inode);
  }

  int start_i = pos / BLOCK_SIZE;        // 起始块的编号
  int end_i = (pos + len) / BLOCK_SIZE;  // 结束块的编号
  int start_pos = pos % BLOCK_SIZE;      // 偏移量
  int w_size = 0;                        // 实际写入的字节数

  if (pos + len > MAX_FILE_SIZE) {
    fprintf(stderr, "文件过大，将被截断\n");
    len -= pos + len - MAX_FILE_SIZE;
  }

  if (len <= 0) {
    fprintf(stderr, "写入小于等于0，无效写入\n");
    return 0;
  }

  for (int i = start_i; i <= end_i && len > 0; ++i) {
    int b = -1;
    dataBlock *block = getBlock(n, i, &b);

    if (block == nullptr || b <= 0 || b >= MAX_BLOCK_NUMBER) {
      break;
    }

    int s = std::min(BLOCK_SIZE, len);
    memcpy(block->content + start_pos, buf + w_size, s);
    start_pos += s;
    start_pos %= BLOCK_SIZE;

    LOG("写入块%d\n", b);
    PutBlock(b, true);
    len -= s;
    w_size += s;
  }

  n->length = std::max(pos + w_size, n->length);
  PutInode(index, true);

  if (n->second_index > 0) {
    PutBlock(n->second_index, true);
  }

  LOG("共写入%d字节\n", w_size);
  return w_size;
}

int Append(int index, int len, const char *buf) {
  if (index <= 0) {
    return 0;
  }

  inode *n = GetInode(index);

  if (n->type == LINK_TYPE) {
    n = GetInode(n->link_inode);
  }

  return Write(n->id, n->length, len, buf);
}

// 实现了文件是字节数组的抽象。
int Read(int index, int pos, int len, char *buf) {
  if (index <= 0) {
    return 0;
  }
  inode *n = GetInode(index);

  if (n->type == LINK_TYPE) {
    n = GetInode(n->link_inode);
  }

  if (pos + len > MAX_FILE_SIZE) {
    fprintf(stderr, "文件过大，读取将被截断\n");
    len -= pos + len - MAX_FILE_SIZE;
  }

  if (len <= 0) {
    fprintf(stderr, "读取小于等于0，无效读取\n");
    return 0;
  }

  int start_i = pos / BLOCK_SIZE;
  int end_i = (pos + len) / BLOCK_SIZE;
  int start_pos = pos % BLOCK_SIZE;
  int r_size = 0;

  for (int i = start_i; i <= end_i && len > 0; ++i) {
    int b = -1;
    dataBlock *block = getBlock(n, i, &b);  // 可能会分配新的块

    if (block == nullptr || b <= 0 || b >= MAX_BLOCK_NUMBER) {
      break;
    }

    int s = std::min(BLOCK_SIZE, len);
    memcpy(buf + r_size, block->content + start_pos, s);

    LOG("读取块%d\n", b);
    PutBlock(b, true);
    start_pos += s;
    start_pos %= BLOCK_SIZE;
    len -= s;
    r_size += s;
  }

  if (n->second_index > 0) {
    PutBlock(n->second_index, true);
  }

  LOG("共读取%d字节\n", r_size);
  return r_size;
}

// 通过字节数组的抽象，这里实现了文件是一个一个entry的抽象
int ReadEntry(int index, int p, int size, char *buf) {
  int pos = p * size;
  int len = size;
  return Read(index, pos, len, buf);
}

int WriteEntry(int index, int p, int size, const char *buf) {
  int pos = p * size;
  int len = size;
  return Write(index, pos, len, buf);
}

// 删除一个文件，释放block块。
bool RemoveFile(int index) {
  inode *n = GetInode(index);

  for (int i = 0; i < MAX_FIRST_INDEX; ++i) {
    if (n->first_index[i] > 0) {
      memset(GetBlock(n->first_index[i]), 0, BLOCK_SIZE);
      PutBlock(n->first_index[i], true);
      ReleaseDataBlock(n->first_index[i]);
    }
  }

  if (n->type == FILE_TYPE && n->second_index > 0) {
    indexBlock *b = GetIndexBlock(n->second_index);
    for (int i = 0; i < MAX_SECOND_INDEX; ++i) {
      if (b->data_block[i] > 0) {
        memset(GetBlock(b->data_block[i]), 0, BLOCK_SIZE);
        PutBlock(b->data_block[i], true);
        ReleaseDataBlock(b->data_block[i]);
      }
    }

    memset(b, 0, BLOCK_SIZE);
    PutBlock(n->second_index, true);
  }

  memset(n, 0, INODE_SIZE);
  PutInode(index, true);
  // ReleaseDataBlock(index); // 修改之后，first_index[0]指向的块是inode节点，所以不要重复释放
  return true;
}