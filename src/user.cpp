#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "head.h"
#include "print.h"

// 树形用户管理。
// 上可以读写下。下只可以读上。
// 通过在用户信息中添加一个parent字段来实现。
userEntry user_info;

static bool get_parent(const char *worker, char *buf) {
  inode *n = GetInode(GetSuperBlock()->user_info_id);
  int len = n->length / sizeof(userEntry);

  for (int i = 0; i < len; ++i) {
    userEntry entry;
    ReadEntry(n->id, i, sizeof(userEntry), (char *)&entry);

    if (strcmp(entry.user_name, worker) == 0) {
      memcpy(buf, entry.parent, sizeof(entry.parent));
      return true;
    }
  }

  return false;
}

// 检查用户名和密码是否匹配
static bool check(const char *name, const char *passwd) {
  inode *n = GetInode(GetSuperBlock()->user_info_id);
  int len = n->length / sizeof(userEntry);

  for (int i = 0; i < len; ++i) {
    userEntry entry;
    ReadEntry(n->id, i, sizeof(userEntry), (char *)&entry);

    if (strcmp(entry.user_name, name) == 0) {
      if (strcmp(passwd, entry.user_passwd) == 0) {
        memcpy(&user_info, &entry, sizeof(userEntry));
        return true;
      } else {
        return false;
      }
    }
  }

  fprintf(stderr, "用户不存在\n");
  return false;
}

// 检查一个用户是否存在，并返回下标
static int exist(const char *name) {
  inode *n = GetInode(GetSuperBlock()->user_info_id);
  int len = n->length / sizeof(userEntry);
  int i;

  for (i = 0; i < len; ++i) {
    userEntry entry;
    ReadEntry(n->id, i, sizeof(userEntry), (char *)&entry);

    if (strcmp(entry.user_name, name) == 0) {
      return i;
    }
  }

  return -1;
}

// 把一个用户的所有孩子的父亲，修改为给定的父亲
static void change_parent(const char *name, const char *parent) {
  inode *n = GetInode(GetSuperBlock()->user_info_id);
  int len = n->length / sizeof(userEntry);

  for (int i = 0; i < len; ++i) {
    userEntry entry;
    ReadEntry(n->id, i, sizeof(userEntry), (char *)&entry);

    if (strcmp(entry.parent, name) == 0) {
      memcpy(entry.parent, parent, sizeof(entry.parent));
      WriteEntry(n->id, i, sizeof(userEntry), (const char *)&entry);
    }
  }
}

bool Exist(const char *name) { return exist(name) >= 0; }

bool LogIn(const char *name, const char *passwd) {
  std::string n, p;

  if (name != nullptr) {
    n = name;
  } else {
    std::cout << "请输入用户名: " << std::flush;
    std::cin >> n;
  }

  if (passwd != nullptr) {
    p = passwd;
  } else {
    std::cout << "请输入密码: " << std::flush;
    std::cin >> p;
  }

  if (check(n.c_str(), p.c_str()) == false) {
    fprintf(stderr, "密码不匹配\n");
    return false;
  }

  return true;
}

const userEntry *GetCurrentUser() { return &user_info; }

// 检查worker是否有权限操作owner的文件
bool Validate(const char *owner, const char *worker) {
  // 特殊处理初始化阶段。
  if (strlen(worker) == 0 || strlen(owner) == 0) {
    return true;
  }

  char owner_buf[MAX_NAME_LENGTH];

  // root的父亲是root，所以可以打断loop
  // 如果owner的祖先中，不存在worker，说明worker无权限访问owner的文件。
  while (strcmp(owner, worker) != 0 && strcmp(owner, "root") != 0) {
    char buf[MAX_NAME_LENGTH];
    memset(buf, 0, MAX_NAME_LENGTH);
    get_parent(owner, buf);
    memcpy(owner_buf, buf, MAX_NAME_LENGTH);
    owner = owner_buf;
  }

  return strcmp(owner, worker) == 0;
}

// 新增用户就是在user_info中添加一个userEntry。
bool UserAdd(const char *name, const char *passwd, const char *parent) {
  if (exist(name) >= 0) {
    fprintf(stderr, "该用户已存在.\n");
    return false;
  }

  int name_len = strlen(name);
  int passwd_len = strlen(passwd);
  int parent_len = strlen(parent);

  if (name_len >= MAX_NAME_LENGTH || passwd_len >= MAX_NAME_LENGTH ||
      parent_len >= MAX_NAME_LENGTH) {
    fprintf(stderr, "用户名、密码或父用户名称过长\n");
    return false;
  }

  userEntry entry;
  memset(&entry, 0, sizeof(userEntry));
  memcpy(entry.user_name, name, name_len);
  memcpy(entry.user_passwd, passwd, passwd_len);
  memcpy(entry.parent, parent, parent_len);
  // 把信息append到user_info_id中
  Append(GetSuperBlock()->user_info_id, sizeof(entry), (const char *)&entry);
  return true;
}

// 删除用户就是把user_info中对应的userEntry清空，并把它的孩子的父亲改为它的父亲。
// 但是需要判断是否允许删除。
bool UserDel(const char *name) {
  int index = exist(name);

  if (index < 0) {
    fprintf(stderr, "该用户不存在\n");
    return true;
  }

  if (strcmp(name, user_info.user_name) == 0) {
    fprintf(stderr, "不可以删除当前用户\n");
    return false;
  }

  if (Validate(name, GetCurrentUser()->user_name) == false) {
    fprintf(stderr, "无删除权限\n");
    return false;
  }

  userEntry del_user;
  ReadEntry(GetSuperBlock()->user_info_id, index, sizeof(userEntry), (char *)&del_user);

  // root -> b -> a -> c -> d
  change_parent(name, del_user.parent);
  userEntry entry;
  memset(&entry, 0, sizeof(entry));
  // 覆盖对应位置。
  WriteEntry(GetSuperBlock()->user_info_id, index, sizeof(entry), (const char *)&entry);
  return true;
}

bool ShowUsers() {
  inode *n = GetInode(GetSuperBlock()->user_info_id);
  int len = n->length / sizeof(userEntry);
  int i;

  for (i = 0; i < len; ++i) {
    userEntry entry;
    ReadEntry(n->id, i, sizeof(userEntry), (char *)&entry);
    if (strlen(entry.user_name) > 0) {
      PRINT_FONT_YEL;
      fprintf(stdout, "[name]%s [parent]%s\n", entry.user_name, entry.parent);
      PRINT_FONT_BLA;
    }
  }

  return true;
}