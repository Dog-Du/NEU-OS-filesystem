#include <string.h>
#include <iostream>
#include "head.h"

static dataBlock *getBlock(inode *n, int i, int *index) {
  if (i < MAX_FIRST_INDEX) {
    n->first_index[i] = (n->first_index[i] <= 0 ? AllocDataBlock() : n->first_index[i]);
    *index = n->first_index[i];
    return (n->first_index[i] <= 0 ? nullptr : GetBlock(n->first_index[i]));
  } else {
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

    indexBlock *block = GetIndexBlock(n->second_index);
    i -= MAX_FIRST_INDEX;
    block->data_block[i] = (block->data_block[i] <= 0 ? AllocDataBlock() : block->data_block[i]);
    *index = block->data_block[i];
    return (block->data_block[i] <= 0 ? nullptr : GetBlock(block->data_block[i]));
  }
  return nullptr;
}

int NewFile(file_type type, const char *file_name, const char *owner_name) {
  int index = AllocDataBlock();
  if (index <= 0) {
    return index;
  }

  inode *n = GetInode(index);
  memset(n, 0, INODE_SIZE);
  n->type = type;
  n->link_cnt = 1;
  n->id = index;

  memcpy(n->file_name, file_name, strlen(file_name));
  memcpy(n->owner_name, owner_name, strlen(owner_name));

  PutInode(index, true);
  return index;
}

int Write(int index, int pos, int len, const char *buf) {
  if (GetSuperBlock()->user_info_id != index && ValidateCurrent(index) == false) {
    fprintf(stderr, "无权限\n");
    return 0;
  }
  inode *n = GetInode(index);

  if (n->type == FILE_TYPE && IsOpen(index) == 0) {
    fprintf(stderr, "未打开文件\n");
    return 0;
  }

  if (index <= 0) {
    return 0;
  }

  if (n->type == LINK_TYPE) {
    n = GetInode(n->link_inode);
  }

  int start_i = pos / BLOCK_SIZE;
  int end_i = (pos + len) / BLOCK_SIZE;
  int start_pos = pos % BLOCK_SIZE;
  int w_size = 0;

  if (pos + len > MAX_FILE_SIZE) {
    fprintf(stderr, "文件过大，将被截断\n");
    len -= pos + len - MAX_FILE_SIZE;
  }

  if (len <= 0) {
    fprintf(stderr, "写入小于等于0，无效写入\n");
    return 0;
  }

  for (int i = start_i; i <= end_i && len > 0; ++i) {
    int index = -1;
    dataBlock *block = getBlock(n, i, &index);

    if (block == nullptr || index <= 0 || index >= MAX_BLOCK_NUMBER) {
      break;
    }

    int s = std::min(BLOCK_SIZE, len);
    memcpy(block->content + start_pos, buf + w_size, s);
    start_pos += s;
    start_pos %= BLOCK_SIZE;

    PutBlock(index, true);
    len -= s;
    w_size += s;
  }

  n->length = std::max(pos + w_size, n->length);
  PutInode(index, true);
  if (n->second_index > 0) {
    PutBlock(n->second_index, true);
  }

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
    dataBlock *block = getBlock(n, i, &b);

    if (block == nullptr || b <= 0 || b >= MAX_BLOCK_NUMBER) {
      break;
    }

    int s = std::min(BLOCK_SIZE, len);
    memcpy(buf + r_size, block->content + start_pos, s);

    start_pos += s;
    start_pos %= BLOCK_SIZE;
    len -= s;
    r_size += s;
  }

  return r_size;
}

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

bool RemoveFile(int index) {
  inode *n = GetInode(index);

  for (int i = 0; i < MAX_FIRST_INDEX; ++i) {
    if (n->first_index[i] > 0) {
      ReleaseDataBlock(n->first_index[i]);
    }
  }

  if (n->type != DIR_TYPE && n->second_index > 0) {
    indexBlock *b = GetIndexBlock(n->second_index);
    for (int i = 0; i < sizeof(b->data_block) / sizeof(b->data_block[0]); ++i) {
      if (b->data_block[i] > 0) {
        ReleaseDataBlock(b->data_block[i]);
      }
    }
  }

  memset(n, 0, INODE_SIZE);
  PutInode(index, true);
  ReleaseDataBlock(index);
  return true;
}