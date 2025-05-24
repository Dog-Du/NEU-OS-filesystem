#include <iostream>
#include "head.h"
#include "print.h"

using namespace std;

void print() {
  cout << "--------------------------------------------------------------------" << endl;
}

// 新增功能：load、拷贝、链接、重命名、移动、多进程访问、树状用户管理、随写随刷保证一致性。
void MainPage()  // 主页信息
{
  PRINT_FONT_YEL;
  cout << "--------------------------command list-----------------------------\n";
  cout << "---close file_name----------------关闭文件\n";
  cout << "---copy name des_director_name----复制文件到目录\n";
  cout << "---create file_name---------------建立文件\n";
  cout << "---deldir director_name-----------删除文件夹\n";
  cout << "---delfile file_name--------------删除文件\n";
  cout << "---dir----------------------------显示当前目录中的目录和文件\n";
  cout << "---logout-------------------------保存结果并退出系统\n";
  cout << "---ltdir -------------------------返回上一级目录\n";
  cout << "---mkdir director_name------------建立目录\n";
  cout << "---ntdir director_name------------进入下一级目录\n";
  cout << "---open file_name-----------------打开文件\n";
  cout << "---read file_name-----------------读文件\n";
  cout << "---rename old_name new_name-------重命名\n";
  cout << "---append file_name content-------追加文件\n";
  cout << "---write file_name pos content----写文件\n";
  cout << "---link src_file dst_file---------链接文件\n";
  cout << "---move src_file dst_dir----------移动文件\n";
  cout << "---useradd user_name passwd-------新增用户\n";
  cout << "---userdel user_name--------------删除用户\n";
  cout << "---login user_name----------------登录其他用户\n";
  cout << "---users--------------------------显示所有用户\n";
  cout << "---load src_file dst_file---------从本地文件系统导入文件\n";
  PRINT_FONT_BLA;
}

void CurrentDirector()  // 显示当前目录
{
  PRINT_FONT_RED;
  printf("%s", GetCurrentUser()->user_name);
  PRINT_FONT_YEL;
  printf(":");
  PRINT_FONT_GRE;
  printf("%s", NowDir());
  PRINT_FONT_BLA;
  printf(">");
}

int main() {
  string command;

  char ch;

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
    string param;

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
    } else if (command == "ntdir") {
      cin >> param;
      NextDir(param.c_str());
    } else if (command == "ltdir") {
      LastDir();
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
      string temp;
      cin >> param >> x >> temp;
      Write(Open(param.c_str()), x, temp.size(), temp.c_str());
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
    } else {
      cout << "错误指令，请重新输入" << endl;
      while (1) {
        char ch;
        ch = getchar();
        if (ch == '\n') {
          break;
        }
      }
    }
  }

  CloseFileSystem();
  getchar();
  return 0;
}