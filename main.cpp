#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include "head.h"
#include "print.h"

using namespace std;

void print() {
  cout << "--------------------------------------------------------------------" << endl;
}

/*
支持功能：
1. `Load` 导入本地文件
2. 硬链接
3. 拷贝
4. 重命名
5. 移动
6. 树状用户管理
7. 随写随刷保证一致性
8. 共享内存实现多进程共享
9. 用信号量上超大粒度锁避免竞争
*/
void MainPage()  // 主页信息
{
  PRINT_FONT_YEL;
  cout << "--------------------------command list-----------------------------\n";
  cout << "---close file_name----------------关闭文件\n";
  cout << "---create file_name---------------建立文件\n";
  cout << "---deldir director_name-----------删除文件夹\n";
  cout << "---delfile file_name--------------删除文件\n";
  cout << "---dir----------------------------显示当前目录中的目录和文件\n";
  cout << "---cd dir_name -------------------改变当前目录\n";
  cout << "---mkdir director_name------------建立目录\n";
  cout << "---open file_name-----------------打开文件\n";
  cout << "---read file_name-----------------读文件\n";
  cout << "---rename old_name new_name-------重命名\n";
  cout << "---append file_name content-------追加文件\n";
  cout << "---write file_name pos content----写文件\n";
  cout << "---link src_file dst_file---------链接文件\n";
  cout << "---move src_file dst_dir----------移动文件\n";
  cout << "---copy name des_director_name----复制文件到目录\n";
  cout << "---useradd user_name passwd-------新增用户\n";
  cout << "---userdel user_name--------------删除用户\n";
  cout << "---login user_name----------------登录其他用户\n";
  cout << "---logout-------------------------退出系统\n";
  cout << "---users--------------------------显示所有用户\n";
  cout << "---clear--------------------------清空屏幕\n";
  cout << "---load src_file dst_file---------从本地文件系统导入文件\n";
  cout << "---log----------------------------开关日志\n";
  cout << "---help---------------------------显示当前页面\n";
  PRINT_FONT_BLA;
}

void CurrentDirector()  // 显示当前目录
{
  PRINT_FONT_RED;
  printf("%s", GetCurrentUser()->user_name);
  PRINT_FONT_YEL;
  printf(":");
  PRINT_FONT_GRE;
  printf("%s", GetPath().c_str());
  PRINT_FONT_BLA;
  printf(">");
}

int main() {
  int fd = open("./process_shared", O_RDWR | O_CREAT | O_TRUNC, 0b111111111);
  ftruncate(fd, sizeof(context));

  context *mutex =
      (context *)mmap(NULL, sizeof(context), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mutex == MAP_FAILED) {
    fprintf(stderr, "系统初始化失败.\n");
    return 0;
  }

  if (mutex->flag == false) {
    mutex->flag = true;
    sem_init(&mutex->mutex, 1, 1);
  }

  string command;

  char ch;

  sem_wait(&mutex->mutex);
  while (1) {
    cout << "是否初始化文件系统，若初始化，则之前的信息将消失!  Y/N" << endl;
    cin >> ch;
    if (ch == 'Y' || ch == 'y') {
      FormatFileSystem(root_path);
      break;
    } else {
      if (ch == 'N' || ch == 'n') {
        OpenFileSystem(root_path);
        break;
      } else {
        cout << "输入有误，请处输入Y/N" << endl;
      }
    }
    print();
  }
  sem_post(&mutex->mutex);

  while (LogIn() == false) {
    ;
  }

  system("clear");
  cout << "登陆成功！欢迎您，" << GetCurrentUser()->user_name << endl;
  MainPage();

  while (true) {
    print();
    CurrentDirector();  // 显示当前目录
    cin >> command;

    if (Exist(GetCurrentUser()->user_name) == false) {
      fprintf(stderr, "当前用户不存在，请重新登录。\n");
      break;
    }

    if (GetPath().empty()) {
      fprintf(stderr, "当前路径不存在，请重新登录。\n");
      break;
    }
    string param;

    sem_wait(&mutex->mutex);
    if (command == "help") {
      MainPage();
    } else if (command == "link") {
      string src, dst;
      cin >> src >> dst;
      Link(src.c_str(), dst.c_str());
    } else if (command == "mkdir") {
      cin >> param;
      CreateDir(param.c_str());
    } else if (command == "dir") {
      ShowDir();
    } else if (command == "cd") {
      cin >> param;
      if (param == ".") {
        ;  // do nothing.
      } else if (param == "..") {
        LastDir();
      } else {
        NextDir(param.c_str());
      }
    } else if (command == "create") {
      cin >> param;
      CreateFile(param.c_str());
    } else if (command == "delfile") {
      cin >> param;
      DeleteFile(param.c_str());
    } else if (command == "deldir") {
      cin >> param;
      DeleteDir(param.c_str());
    } else if (command == "append") {
      cin >> param;
      string temp;
      cin >> temp;
      Append(Open(param.c_str()), temp.size(), temp.c_str());
    } else if (command == "write") {
      int x;
      string temp, num;
      cin >> param >> num >> temp;

      try {
        x = atoi(num.c_str());

        if (x < 0 || num.empty() || !isdigit(num.front())) {
          throw invalid_argument("Position must be non-negative.");
        }

        int fd = Open(param.c_str());

        if (fd <= 0 || GetInode(fd)->type != FILE_TYPE) {
          fprintf(stderr, "文件未打开或不是文件类型。\n");
          throw invalid_argument("Invalid file type.");
        }
        Write(Open(param.c_str()), x, temp.size(), temp.c_str());
      } catch (const invalid_argument &e) {
        fprintf(stderr, "错误：%s\n", e.what());
      } catch (...) {
        fprintf(stderr, "错误：位置必须为非负整数。\n");
      }
    } else if (command == "open") {
      cin >> param;
      OpenFile(param.c_str());
    } else if (command == "read") {
      cin >> param;
      ReadFile(param.c_str());
    } else if (command == "close") {
      cin >> param;
      CloseFile(param.c_str());
    } else if (command == "logout") {
      sem_post(&mutex->mutex);
      break;
    } else if (command == "rename") {
      cin >> param;
      string str;
      cin >> str;
      Rename(param.c_str(), str.c_str());
    } else if (command == "copy") {
      cin >> param;
      string str;
      cin >> str;
      Copy(param.c_str(), str.c_str());
    } else if (command == "useradd") {
      string name, passwd;
      cin >> name >> passwd;
      UserAdd(name.c_str(), passwd.c_str(), GetCurrentUser()->user_name);
    } else if (command == "userdel") {
      string name;
      cin >> name;
      UserDel(name.c_str());
    } else if (command == "users") {
      ShowUsers();
    } else if (command == "login") {
      string name;
      cin >> name;
      LogIn(name.c_str());
    } else if (command == "move") {
      string file, dst;
      cin >> file >> dst;
      Move(file.c_str(), dst.c_str());
    } else if (command == "load") {
      string src, dst;
      cin >> src >> dst;
      Load(src.c_str(), dst.c_str());
    } else if (command == "clear") {
      system("clear");
    } else if (command == "log") {
      need_log = !need_log;
      if (need_log) {
        cout << "日志已开启" << endl;
      } else {
        cout << "日志已关闭" << endl;
      }
    } else {
      cout << "错误指令，请重新输入" << endl;
    }

    while (1) {
      char ch;
      ch = getchar();
      if (ch == '\n') {
        break;
      }
    }

    sem_post(&mutex->mutex);
  }

  sem_wait(&mutex->mutex);
  CloseFileSystem();
  sem_post(&mutex->mutex);
  getchar();
  return 0;
}