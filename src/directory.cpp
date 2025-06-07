#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cassert>
#include <list>
#include <set>
#include <string>
#include "head.h"
#include "print.h"

int current_dir_index = -1;
std::set<int> open_file;
const char *TYPE2NAME[] = {"FILE", "DIRE", "LINK", "USER"};

static void init() {
  static bool is_init = false;
  if (is_init == false) {
    current_dir_index = GetSuperBlock()->root_dir_id;
    is_init = true;
  }
}

// 检查权限
static bool check(int index = current_dir_index) {
  init();
  return ValidateCurrent(current_dir_index);
}

// 检查当前目录下是否有指定文件
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

// 当前用户是否有权限访问指定 inode
bool ValidateCurrent(int index) {
  return Validate(GetInode(index)->owner_name, GetCurrentUser()->user_name);
}

// 读取目录，写入files和len中
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

// 创建一个新文件
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

  // 把新文件添加到当前目录
  Append(current_dir_index, sizeof(dirEntry), (const char *)&entry);
  open_file.insert(index);
  return true;
}

// 获取当前目录下的指定文件的index，pos表示目录中的位置
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

// 删除一个文件。
bool DeleteFile(const char *file_name) {
  int pos = -1;
  int fd = Open(file_name, &pos);
  if (fd < 0 || pos < 0 || GetInode(fd)->type == DIR_TYPE) {
    fprintf(stderr, "不存在的文件。\n");
    return true;
  }

  // 不需要检查目录是否有权限，因为不可能在一个没有权限的目录下创建一个有权限的文件。
  if (ValidateCurrent(fd) == false) {
    fprintf(stderr, "无权限\n");
    return false;
  }

  if (strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0) {
    fprintf(stderr, "被占用的目录名\n");
    return false;
  }

  inode *n = GetInode(fd);
  inode *nn = nullptr;  // 如果文件为链接位置，则nn指向源文件。
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

  // 如果链接为0，则删除源文件。
  if (need_del) {
    RemoveFile(nn != nullptr ? nn->id : n->id);
    fprintf(stdout, "删除文件%s\n", file_name);
  } else {
    fprintf(stdout, "删除链接%s %d\n", file_name, n->id);
  }

  if (nn != nullptr) {
    RemoveFile(n->id);
    PutInode(nn->id, true);
  }

  PutInode(n->id, true);
  open_file.erase(nn != nullptr ? nn->id : n->id);
  return true;
}

// 递归检查文件夹是否有权限，如果文件夹下任意一个文件没有权限，则无法删除这个文件夹。
static bool validate_dir(const char *dir_name) {
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

  inode *d = GetInode(fd);
  if (d->type != DIR_TYPE) {
    fprintf(stderr, "该文件不是目录");
    return true;
  }

  // 进入目录
  NextDir(d->file_name);
  const int num = d->length / sizeof(dirEntry);

  int i;
  for (i = 0; i < num; ++i) {
    dirEntry entry;
    ReadEntry(fd, i, sizeof(dirEntry), (char *)&entry);

    if (entry.file_id > 0) {
      inode *n = GetInode(entry.file_id);
      if (ValidateCurrent(n->id) == false) {
        break;
      } else if (n->type == DIR_TYPE && validate_dir(n->file_name) == false) {
        break;
      }
    }
  }

  LastDir();        // 退出目录
  return i == num;  // 所有文件都通过验证
}

bool DeleteDir(const char *dir_name) {
  // 递归检查所有目录下所有文件。
  if (validate_dir(dir_name) == false) {
    fprintf(stderr, "无权限\n");
    return false;
  }

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

  // 进入目录
  NextDir(d->file_name);
  const int num = d->length / sizeof(dirEntry);

  for (int i = 0; i < num; ++i) {
    dirEntry entry;
    ReadEntry(fd, i, sizeof(dirEntry), (char *)&entry);

    // 递归删除
    if (entry.file_id > 0) {
      inode *n = GetInode(entry.file_id);
      if (n->type == DIR_TYPE) {
        DeleteDir(n->file_name);  // 递归。
      } else {
        DeleteFile(n->file_name);  // 删除文件。
      }
    }
  }

  LastDir();  // 退出目录。

  dirEntry entry;
  entry.file_id = -1;
  WriteEntry(current_dir_index, pos, sizeof(dirEntry), (const char *)&entry);
  PutInode(d->id, true);
  return RemoveFile(fd);  // 删除这个目录
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
  if (fd < 0) {
    return false;
  }

  // 创建文件夹。
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
  inode *n = GetInode(current_dir_index);

  if (n->last_dir <= 0) {
    fprintf(stderr, "已到根目录\n");
    return false;
  }
  current_dir_index = n->last_dir;
  return true;
}

// 打印出来目录下所有项。
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
      fprintf(stdout, "[type]%s ", TYPE2NAME[n->type]);
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

// 获取当前路径
std::string GetPath() {
  init();

  int cur = current_dir_index;

  if (GetInode(cur)->last_dir <= 0 && cur != GetSuperBlock()->root_dir_id) {
    return "";
  }

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

// 链接
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
  // 链接的链接，只应该链接到源文件。
  while (old->type == LINK_TYPE) {
    i = old->link_inode;
    old = GetInode(i);
  }

  if (old->type != FILE_TYPE) {
    fprintf(stderr, "暂不支持该类型链接\n");
    return false;
  }

  int index = NewFile(LINK_TYPE, dst, GetCurrentUser()->user_name);
  if (index <= 0) {
    return false;
  }

  open_file.insert(index);
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

// 重命名
bool Rename(const char *old_name, const char *new_name) {
  int i = has_file(current_dir_index, old_name);
  if (i < 0) {
    fprintf(stderr, "不存在该文件\n");
    return false;
  }

  if (ValidateCurrent(i) == false) {
    fprintf(stderr, "无权限\n");
    return false;
  }

  int j = has_file(current_dir_index, new_name);
  if (j >= 0) {
    fprintf(stderr, "文件已存在\n");
    return false;
  }

  int len = strlen(new_name);

  if (len >= MAX_NAME_LENGTH) {
    fprintf(stderr, "文件名过长\n");
    return false;
  }
  inode *n = GetInode(i);
  memset(n->file_name, 0, MAX_NAME_LENGTH);

  // 修改文件名。
  memcpy(n->file_name, new_name, len);
  PutInode(n->id, true);
  return true;
}

// 拷贝
bool Copy(const char *file, const char *dir) {
  int pos = -1;
  int i = has_file(current_dir_index, file, &pos);
  if (i < 0 || GetInode(i)->type == DIR_TYPE) {
    fprintf(stderr, "不存在该文件\n");
    return false;
  }

  int j = has_file(current_dir_index, dir);
  // j < 0 说明不在复制的文件夹不在当前目录下
  if (j < 0) {
    // 如果还不是 .. 则不存在
    if (strcmp(dir, "..") != 0) {
      fprintf(stderr, "不存在目录\n");
      return false;
    }

    inode *n = GetInode(current_dir_index);

    if (n->last_dir < 0) {
      fprintf(stderr, "已在根目录\n");
      return false;
    }

    j = n->last_dir;  // 目标文件夹时 n 的上一级目录
  }

  // 如果目标文件夹已经有了同名文件，则不能拷贝。
  if (has_file(j, file) >= 0) {
    fprintf(stderr, "目录中已有该文件\n");
    return false;
  }

  // 把文件 i 拷贝到文件夹 j 中。
  inode *f = GetInode(i);
  int index = NewFile(f->type, f->file_name, GetCurrentUser()->user_name);
  if (index <= 0) {
    return false;
  }
  inode *n = GetInode(index);
  open_file.insert(n->id);

  // 如果 f 被拷贝文件LINK，不需要则不需要把源文件也拷贝
  if (f->type == LINK_TYPE) {
    n->link_inode = f->link_inode;
    inode *nn = GetInode(n->link_inode);
    nn->link_cnt += 1;
    n->link_cnt = nn->link_cnt;
    PutInode(nn->id, true);
  } else {
    // 把文件内容拷贝了。
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

// 移动
bool Move(const char *file, const char *dir) {
  int pos = -1;
  int i = has_file(current_dir_index, file, &pos);
  if (i < 0 || GetInode(i)->type == DIR_TYPE) {
    fprintf(stderr, "不存在该文件\n");
    return false;
  }

  if (ValidateCurrent(i) == false) {
    fprintf(stderr, "无权限\n");
    return false;
  }

  // 这部分和copy说一样的逻辑，寻找目标文件夹。
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

  // 把文件 i 移动到文件夹 j 中。
  // 移动，只需要在原来的文件夹中删除index，在新文件夹中增加index即可。
  dirEntry entry;
  entry.file_id = -1;
  WriteEntry(current_dir_index, pos, sizeof(dirEntry), (const char *)&entry);
  entry.file_id = i;
  Append(j, sizeof(entry), (const char *)&entry);
  return true;
}

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

  char *s = (char *)alloca(n->length + 1);
  memset(s, 0, n->length + 1);

  // 读取
  int len = Read(fd, 0, n->length, s);
  s[len] = '\0';

  // 按字节打印，避免因为'\0'出现问题。
  PRINT_FONT_GRE
  for (int i = 0; i < len; ++i) {
    putc(s[i], stdout);
  }
  PRINT_FONT_RED
  fprintf(stdout, "\n共读取%d字节\n", len);
  PRINT_FONT_BLA;
}