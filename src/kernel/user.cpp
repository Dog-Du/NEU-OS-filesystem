#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include "head.h"
#include "print.h"

userEntry user_info;

static bool get_parent(const char *worker, char *buf) {
  inode *n = GetInode(GetSuperBlock()->user_info_id);
  int len = n->length / sizeof(userEntry);

  for (int i = 0; i < len; ++i) {
    userEntry entry;
    ReadEntry(n->id, i, sizeof(userEntry), (char *)&entry);

    if (strcmp(entry.user_name, worker) == 0) {
      memcpy(buf, entry.user_name, strlen(entry.user_name));
      return true;
    }
  }

  return false;
}

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

bool Validate(const char *owner, const char *worker) {
  if (strlen(worker) == 0 || strlen(owner) == 0) {
    return true;
  }

  char buf[MAX_NAME_LENGTH];

  while (strcmp(owner, worker) != 0 && strcmp(owner, "root") != 0) {
    memset(buf, 0, MAX_NAME_LENGTH);
    get_parent(owner, buf);
    owner = buf;
  }

  return strcmp(owner, worker) == 0;
}

bool UserAdd(const char *name, const char *passwd, const char *parent) {
  if (exist(name) >= 0) {
    fprintf(stderr, "该用户已存在.\n");
    return false;
  }

  userEntry entry;
  memset(&entry, 0, sizeof(userEntry));
  memcpy(&entry.user_name, name, strlen(name));
  memcpy(&entry.user_passwd, passwd, strlen(passwd));
  memcpy(&entry.parent, parent, strlen(parent));
  Append(GetSuperBlock()->user_info_id, sizeof(entry), (const char *)&entry);
  return true;
}

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

  userEntry entry;
  memset(&entry, 0, sizeof(entry));
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