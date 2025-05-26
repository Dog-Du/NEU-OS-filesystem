#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include <set>
#include <streambuf>
#include <string>
#include <vector>
#include "head.h"
#include "print.h"

int current_dir_index = -1;
std::set<int> open_file;

static void init() {
  static bool is_init = false;
  if (is_init == false) {
    current_dir_index = GetSuperBlock()->root_dir_id;
    is_init = true;
  }
}

static bool check(int index = current_dir_index) {
  init();
  return ValidateCurrent(current_dir_index);
}

static int has_file(int index, const char *file, int *pos = nullptr) {
  int len = 0;
  int *files = (int *)alloca(GetInode(index)->length);
  int ret = -1;
  ReadDir(index, &len, files);

  for (int i = 0; i < len; ++i) {
    inode *n = GetInode(files[i]);

    if (strcmp(n->file_name, file) == 0) {
      ret = files[i];
      if (pos != nullptr) {
        *pos = i;
      }
      break;
    }
  }

  return ret;
}

bool IsOpen(int fd) { return open_file.count(fd) > 0; }

bool ValidateCurrent(int index) {
  return Validate(GetInode(index)->owner_name, GetCurrentUser()->user_name);
}

bool ReadDir(int index, int *len, int *files) {
  inode *n = GetInode(index);
  constexpr int entry_size = sizeof(dirEntry);
  *len = n->length / entry_size;

  for (int i = 0; i < *len; ++i) {
    dirEntry entry;
    ReadEntry(index, i, entry_size, (char *)&entry);
    files[i] = entry.file_id;
  }

  return true;
}

bool CreateFile(const char *file_name) {
  if (check() == false) {
    fprintf(stderr, "无权限\n");
    return false;
  }

  if (has_file(current_dir_index, file_name) >= 0) {
    fprintf(stderr, "当前目录已经有%s\n", file_name);
    return false;
  }

  if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
    fprintf(stderr, "被占用的目录名\n");
    return false;
  }

  int index = NewFile(FILE_TYPE, file_name, GetCurrentUser()->user_name);
  if (index <= 0) {
    return false;
  }
  dirEntry entry;
  entry.file_id = index;

  Append(current_dir_index, sizeof(dirEntry), (const char *)&entry);
  open_file.insert(index);
  return true;
}

int Open(const char *file_name, int *pos) {
  int fd = has_file(current_dir_index, file_name, pos);

  if (strcmp(file_name, ".") == 0) {
    return current_dir_index;
  }

  if (strcmp(file_name, "..") == 0) {
    return GetInode(current_dir_index)->last_dir;
  }

  if (fd < 0) {
    fprintf(stderr, "当前目录没有此文件\n");
    return -1;
  }
  return fd;
}

int OpenFile(const char *file_name) {
  int fd = Open(file_name);
  if (fd < 0) {
    return 0;
  }

  if (IsOpen(fd)) {
    fprintf(stderr, "文件已打开。\n");
    return fd;
  }

  open_file.insert(fd);
  return fd;
}

bool CloseFile(const char *file_name) {
  int fd = Open(file_name);
  if (fd < 0) {
    fprintf(stderr, "不存在的文件。\n");
    return true;
  }

  if (IsOpen(fd) == false) {
    fprintf(stderr, "未打开的文件。\n");
    return true;
  }
  open_file.erase(fd);
  return true;
}

bool DeleteFile(const char *file_name) {
  int pos = -1;
  int fd = Open(file_name, &pos);
  if (fd < 0 || pos < 0) {
    fprintf(stderr, "不存在的文件。\n");
    return true;
  }

  if (ValidateCurrent(fd) == false) {
    fprintf(stderr, "无权限\n");
    return false;
  }

  if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
    fprintf(stderr, "被占用的目录名\n");
    return false;
  }

  inode *n = GetInode(fd);
  inode *nn = nullptr;
  bool need_del = false;
  if (n->type == LINK_TYPE) {
    nn = GetInode(n->link_inode);
    need_del = (--nn->link_cnt == 0);
  } else {
    need_del = (--n->link_cnt == 0);
  }

  dirEntry entry;
  entry.file_id = -1;
  WriteEntry(current_dir_index, pos, sizeof(dirEntry), (const char *)&entry);

  if (need_del) {
    RemoveFile(nn != nullptr ? nn->id : n->id);
    fprintf(stdout, "删除文件%s\n", file_name);
  } else {
    fprintf(stdout, "删除链接%s\n", file_name);
  }

  if (nn != nullptr) {
    PutInode(nn->id, true);
  }

  PutInode(n->id, true);
  open_file.erase(nn != nullptr ? nn->id : n->id);
  return true;
}

bool DeleteDir(const char *dir_name) {
  int pos = -1;
  int fd = Open(dir_name, &pos);
  if (fd < 0 || pos < 0) {
    fprintf(stderr, "目录不存在\n");
    return true;
  }

  if (ValidateCurrent(fd) == false) {
    fprintf(stderr, "无权限\n");
    return false;
  }

  if (strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0) {
    fprintf(stderr, "被占用的目录名\n");
    return false;
  }

  inode *d = GetInode(fd);
  if (d->type != DIR_TYPE) {
    fprintf(stderr, "该文件不是目录");
    return true;
  }

  NextDir(d->file_name);

  for (int i = 0; i < d->length / sizeof(int); ++i) {
    dirEntry entry;
    ReadEntry(fd, i, sizeof(dirEntry), (char *)&entry);

    if (entry.file_id > 0) {
      inode *n = GetInode(entry.file_id);
      if (n->type == DIR_TYPE) {
        DeleteDir(n->file_name);
      } else {
        DeleteFile(n->file_name);
      }
    }
  }

  LastDir();

  dirEntry entry;
  entry.file_id = -1;
  WriteEntry(current_dir_index, pos, sizeof(dirEntry), (const char *)&entry);
  PutInode(d->id, true);
  return RemoveFile(fd);
}

bool CreateDir(const char *dir_name) {
  if (check() == false) {
    fprintf(stderr, "无权限\n");
    return false;
  }

  if (has_file(current_dir_index, dir_name) >= 0) {
    fprintf(stderr, "目录下已存在相同名字\n");
    return false;
  }

  if (strcmp(dir_name, ".") == 0 || strcmp(dir_name, "..") == 0) {
    fprintf(stderr, "被占用的目录名\n");
    return false;
  }

  int fd = NewFile(DIR_TYPE, dir_name, GetCurrentUser()->user_name);
  inode *n = GetInode(fd);
  n->last_dir = current_dir_index;

  dirEntry entry;
  entry.file_id = fd;
  Append(current_dir_index, sizeof(dirEntry), (const char *)&entry);
  PutInode(n->id, true);
  return true;
}

bool NextDir(const char *dir_name) {
  int fd = has_file(current_dir_index, dir_name);
  if (fd < 0 || GetInode(fd)->type != DIR_TYPE) {
    fprintf(stderr, "无此目录\n");
    return false;
  }

  current_dir_index = fd;
  return true;
}

bool LastDir() {
  if (check() == false) {
    return false;
  }

  inode *n = GetInode(current_dir_index);

  if (n->last_dir <= 0) {
    fprintf(stderr, "已到根目录\n");
    return false;
  }
  current_dir_index = n->last_dir;
  return true;
}

void ShowDir() {
  int len = 0;
  int *files = (int *)alloca(GetInode(current_dir_index)->length);
  ReadDir(current_dir_index, &len, files);

  for (int i = 0; i < len; ++i) {
    dirEntry entry;
    ReadEntry(current_dir_index, i, sizeof(entry), (char *)&entry);

    if (entry.file_id > 0) {
      inode *n = GetInode(entry.file_id);
      inode *nn = nullptr;

      if (n->type == LINK_TYPE) {
        nn = GetInode(n->link_inode);
      }

      PRINT_FONT_YEL;
      fprintf(stdout, "[type]%s ", (n->type == DIR_TYPE ? "d" : "f"));
      PRINT_FONT_GRE;
      fprintf(stdout, "[name]%s ", n->file_name);
      PRINT_FONT_RED;
      fprintf(stdout, "[owner]%s [size]%d [inode]%d [link]%d\n", n->owner_name,
              (nn != nullptr ? nn->length : n->length), n->id,
              (nn != nullptr ? nn->link_cnt : n->link_cnt));
      PRINT_FONT_BLA;
    }
  }

  fprintf(stdout, "\n");
}

const char *NowDir() {
  init();
  inode *n = GetInode(current_dir_index);
  return GetInode(current_dir_index)->file_name;
}

std::string GetPath() {
  init();
  int cur = current_dir_index;
  std::list<std::string> li;
  while (cur != GetSuperBlock()->root_dir_id) {
    inode *n = GetInode(cur);
    std::string s = n->file_name;
    li.push_front(s);
    cur = n->last_dir;
  }

  std::string s = (li.empty() ? "/" : "");
  for (auto &str : li) {
    s += "/" + str;
  }

  return s;
}

bool Link(const char *src, const char *dst) {
  int i = has_file(current_dir_index, src);
  if (i < 0) {
    fprintf(stderr, "不存在该文件\n");
    return false;
  }

  int j = has_file(current_dir_index, dst);
  if (j >= 0) {
    fprintf(stderr, "文件已存在\n");
    return false;
  }
  inode *old = GetInode(i);

  if (old->type != FILE_TYPE) {
    fprintf(stderr, "暂不支持该类型链接\n");
    return false;
  }

  int index = NewFile(LINK_TYPE, dst, GetCurrentUser()->user_name);
  if (index <= 0) {
    return false;
  }

  inode *n = GetInode(index);
  old->link_cnt += 1;
  n->link_inode = i;

  dirEntry entry;
  entry.file_id = index;

  Append(current_dir_index, sizeof(dirEntry), (const char *)&entry);
  PutInode(n->id, true);
  PutInode(old->id, true);
  return true;
}

bool Rename(const char *old_name, const char *new_name) {
  int i = has_file(current_dir_index, old_name);
  if (i < 0) {
    fprintf(stderr, "不存在该文件\n");
    return false;
  }

  int j = has_file(current_dir_index, new_name);
  if (j >= 0) {
    fprintf(stderr, "文件已存在\n");
    return false;
  }

  inode *n = GetInode(i);
  memset(n->file_name, 0, MAX_NAME_LENGTH);
  memcpy(n->file_name, new_name, strlen(new_name));
  PutInode(n->id, true);
  return true;
}

bool Copy(const char *file, const char *dir) {
  int pos = -1;
  int i = has_file(current_dir_index, file, &pos);
  if (i < 0) {
    fprintf(stderr, "不存在该文件\n");
    return false;
  }

  int j = has_file(current_dir_index, dir);
  if (j < 0) {
    if (strcmp(dir, "..") != 0) {
      fprintf(stderr, "不存在目录\n");
      return false;
    }
    inode *n = GetInode(j);

    if (n->type != DIR_TYPE) {
      fprintf(stderr, "不是目录\n");
      return false;
    }

    if (n->last_dir < 0) {
      fprintf(stderr, "已在根目录\n");
      return false;
    }

    j = n->last_dir;
  }

  if (has_file(j, file) >= 0) {
    fprintf(stderr, "目录中已有该文件\n");
    return false;
  }

  // 把 i 拷贝到 j 中。
  inode *f = GetInode(i);
  int index = NewFile(f->type, f->file_name, GetCurrentUser()->user_name);
  if (index <= 0) {
    return false;
  }
  inode *n = GetInode(index);
  open_file.insert(n->id);

  if (f->type == LINK_TYPE) {
    n->link_inode = f->link_inode;
    inode *nn = GetInode(n->link_inode);
    nn->link_cnt += 1;
    n->link_cnt = nn->link_cnt;
    PutInode(nn->id, true);
  } else {
    char *buf = (char *)alloca(f->length);
    Read(f->id, 0, f->length, buf);
    Write(n->id, 0, f->length, buf);
  }

  dirEntry entry;
  entry.file_id = index;
  Append(j, sizeof(entry), (const char *)&entry);
  PutInode(n->id, true);
  PutInode(f->id, true);
  return true;
}

bool Move(const char *file, const char *dir) {
  int pos = -1;
  int i = has_file(current_dir_index, file, &pos);
  if (i < 0) {
    fprintf(stderr, "不存在该文件\n");
    return false;
  }

  if (ValidateCurrent(i) == false) {
    fprintf(stderr, "无权限\n");
    return false;
  }

  int j = has_file(current_dir_index, dir);
  if (j < 0) {
    if (strcmp(dir, "..") != 0) {
      fprintf(stderr, "不存在目录\n");
      return false;
    }
    inode *n = GetInode(current_dir_index);

    if (n->last_dir < 0) {
      fprintf(stderr, "已在根目录\n");
      return false;
    }

    j = n->last_dir;
  }

  if (has_file(j, file) >= 0) {
    fprintf(stderr, "目录中已有该文件\n");
    return false;
  }

  // 把 i 移动到 j 中。
  dirEntry entry;
  entry.file_id = -1;
  WriteEntry(current_dir_index, pos, sizeof(dirEntry), (const char *)&entry);
  entry.file_id = i;
  Append(j, sizeof(entry), (const char *)&entry);
  return true;
}