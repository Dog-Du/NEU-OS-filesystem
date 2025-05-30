#ifndef __HEAD__
#define __HEAD__
#include <semaphore.h>
#include <atomic>
#include <set>
#include <string>

constexpr int MAX_NAME_LENGTH = 32;
constexpr int MAX_PASSWD_LENGTH = 32;
constexpr int INODE_SIZE = 128;
constexpr int BLOCK_SIZE = 4096;                          // 数据块内容大小
constexpr int DISK_SIZE = 50 * 1024 * 1024 + BLOCK_SIZE;  // 50MB 磁盘
constexpr int MEM_SIZE = DISK_SIZE;  // 因为没有缓冲池，为了方便暂时内存 == 磁盘大小。

constexpr int MAX_BLOCK_NUMBER = (DISK_SIZE - BLOCK_SIZE) / (INODE_SIZE + BLOCK_SIZE);
constexpr int MAX_INODE_NUMBER = MAX_BLOCK_NUMBER;
constexpr int INODE_OFFSET = BLOCK_SIZE;
constexpr int DATA_BLOCK_OFFSET = MAX_BLOCK_NUMBER * INODE_SIZE + INODE_OFFSET;
constexpr int MAX_FIRST_INDEX = 11;
constexpr int MAX_SECOND_INDEX = BLOCK_SIZE / sizeof(int);
constexpr int MAX_FILE_SIZE =
    (MAX_FIRST_INDEX + MAX_SECOND_INDEX) * BLOCK_SIZE;  // 4239360 bytes == 4.04296875 MB
constexpr int DIR_ENTRY_NUMBER = MAX_FIRST_INDEX * BLOCK_SIZE / sizeof(int);
constexpr char root_path[] = "./MyFileSystem";

enum file_type : int {
  FILE_TYPE = 0,
  DIR_TYPE,
  LINK_TYPE,
  USER_TYPE,
};

typedef struct inode {
  file_type type;  // 文件类型
  int id;          // inode 的id。
  int length;      // 文件所占字节数
  int link_cnt;    // link_cnt，可拓展为目录也可链接。

  union {
    int second_index;  // 二级索引数据块
    int last_dir;      // 如果 inode 为文件夹，则不使用 second_index。
    int link_inode;    // 如果 inode 为链接文件，则指向原本文件。
  };

  char file_name[MAX_NAME_LENGTH];   // 文件名字
  char owner_name[MAX_NAME_LENGTH];  // 主人名字.
  int first_index[MAX_FIRST_INDEX];  // 一级索引数据域，first_index[0] == id;
  // 文件最大为：(MAX_FIRST_INDEX + MAX_SECOND_INDEX) * BLOCK_SIZE
  // == (MAX_FIRST_INDEX + BLOCK_SIZE / sizeof(int)) * BLOCK_SIZE
  // == MAX_FIRST_INDEX * BLOCK_SIZE + BLOCK_SIZE * BLOCK_SIZE / sizeof(int)
  // == (1/4) * BLOCK_SIZE^2 + MAX_FIRST_INDEX * BLOCK_SIZE
} inode;

static_assert(sizeof(inode) == 128);
static_assert(BLOCK_SIZE % INODE_SIZE == 0);

typedef struct dataBlock  // 数据块
{
  char content[BLOCK_SIZE];  // 数据块内容
} dataBlock;
static_assert(sizeof(dataBlock) == BLOCK_SIZE);

typedef struct indexBlock {
  int data_block[(BLOCK_SIZE) /
                 sizeof(int)];  // 二级索引块，指向数据块的编号。大小应该等于datablock
} indexBlock;

static_assert(sizeof(indexBlock) == BLOCK_SIZE);
static_assert(sizeof(indexBlock) == sizeof(dataBlock));

typedef struct superBlock {
  int user_info_id;  // 用户信息的节点。
  int root_dir_id;   // 根目录的节点。
  int stack_num;     // 超级栈的当前空闲数量
  int stack[(BLOCK_SIZE - sizeof(root_dir_id) - sizeof(stack_num) - sizeof(user_info_id)) /
            sizeof(int)];  // 超级栈
} superBlock;
static_assert(sizeof(superBlock) == BLOCK_SIZE);

typedef struct freeBlock {
  const int _;
  const int __;  // 为了对齐superBlock;
  int stack_num;
  int stack[(BLOCK_SIZE - sizeof(_) - sizeof(__) - sizeof(stack_num)) / sizeof(int)];
  ;  // stack[0] 是成组链接的下一组。
} freeBlock;
static_assert(sizeof(freeBlock) == BLOCK_SIZE);

typedef struct dirEntry {
  int file_id;
} dirEntry;

typedef struct userEntry {
  char user_name[MAX_NAME_LENGTH];
  char user_passwd[MAX_PASSWD_LENGTH];
  char parent[MAX_NAME_LENGTH];
} userEntry;

typedef struct context {
  std::atomic<bool> flag;  // 是否初始化
  sem_t mutex;             // 互斥锁，保证多进程访问共享内存的安全
} context;

/* -------------------全局变量--------------------- */
extern int current_dir_index;  // 当前目录的索引编号 定义在directory.cpp
extern std::set<int> open_file;  // 打开的文件集合，存储文件的索引编号 定义在directory.cpp
extern userEntry user_info;      // 当前用户信息 定义在user.cpp
extern int fd;                   // 文件描述符，用于访问磁盘文件 定义在disk.cpp
extern char *memory;             // 多进程共享内存 定义在disk.cpp
extern const char *TYPE2NAME[];  // 文件类型名称数组 定义在directory.cpp
extern bool need_log;            // 是否需要打印日志，定义在disk.cpp中
/* -------------------全局变量--------------------- */

// 一个简单的宏，用来打印日志。
#define LOG(fmt, ...)                    \
  if (need_log) {                        \
    fprintf(stderr, fmt, ##__VA_ARGS__); \
  }

/* -------------------磁盘操作--------------------- */
extern bool FormatFileSystem(const char *file);  //  格式化文件系统
extern bool OpenFileSystem(const char *file);    // 打开文件系统
extern bool CloseFileSystem();                   // 关闭文件系统
extern superBlock *GetSuperBlock();              // 获取超级块
extern inode *GetInode(int index);               // 获取索引节点
extern dataBlock *GetBlock(int index);           // 获取数据块
extern indexBlock *GetIndexBlock(int index);     // 获取二级索引块
extern void PutBlock(int index, bool write);     // 写入数据块
extern void PutInode(int index, bool write);     // 写入索引节点
extern void PutSuperBlock(bool write);           // 写入超级块
extern int AllocDataBlock();                     // 分配数据编号
extern void ReleaseDataBlock(int index);         // 释放数据编号
// extern void FlushDisk();
/* -------------------磁盘操作--------------------- */

/* -------------------文件操作--------------------- */
// 在index文件的pos位置写入len字节buf内容，最通用的写方法
extern int Write(int index, int pos, int len, const char *buf);
// 在index文件的末尾追加len字节buf内容，基于write实现
extern int Append(int index, int len, const char *buf);
// 在index文件的pos位置读取len字节到buf中，最通用的读方法
extern int Read(int index, int pos, int len, char *buf);
// 在index文件的pos下标读取size的项到buf中，基于read实现，把一个文件看作一个数组
extern int ReadEntry(int index, int pos, int size, char *buf);
// 在index文件的pos下标写入size的项到buf中，基于write实现，把一个文件看作一个数组
extern int WriteEntry(int index, int pos, int size, const char *buf);
// 新建一个文件，返回文件的索引编号
extern int NewFile(file_type type, const char *file_name, const char *owner_name);
extern bool RemoveFile(int index);  // 从文件系统删除一个文件index，返回是否成功
/* -------------------文件操作--------------------- */

/* -------------------文件夹操作------------------- */
// 创建一个文件
extern bool CreateFile(const char *file_name);
// 在当前目录下打开一个文件，pos表示在目录数据项中的位置
extern int Open(const char *file_name, int *pos = nullptr);
// 在当前目录下打开一个文件，返回文件的索引编号，插入进open_file集合中
extern int OpenFile(const char *file_name);
// 关闭一个文件，返回是否成功，从open_file集合中删除
extern bool CloseFile(const char *file_name);
// 删除一个文件，返回是否成功，基于 RemoveFile 实现
extern bool DeleteFile(const char *file_name);
// 删除一个目录，返回是否成功，基于DeleteFile和RemoveFile实现
extern bool DeleteDir(const char *dir_name);
// 在当前目录下创建一个目录，返回是否成功
extern bool CreateDir(const char *dir_name);
// 在当前目录下进入一个目录，返回是否成功
extern bool NextDir(const char *dir_name);
// 读取指定目录项index的所有项进files文件，len和files为返回值
extern bool ReadDir(int index, int *len, int *files);
// 显示当前目录下的内容
extern void ShowDir();
// 返回上一级目录
extern bool LastDir();
// 返回当前目录的名字
extern const char *NowDir();
// 返回当前目录的绝对路径
extern std::string GetPath();
// 重命名
extern bool Rename(const char *old_name, const char *new_name);
// 移动文件到指定目录，返回是否成功
extern bool Move(const char *file, const char *dir);
// 拷贝文件到指定目录，返回是否成功
extern bool Copy(const char *file, const char *dir);
// 检查一个文件是否打开
extern bool IsOpen(int fd);
/* -------------------文件夹操作------------------- */

/* -------------------用户管理--------------------- */
// 检查worker是否有权限操作owner的文件
extern bool Validate(const char *owner, const char *worker);
// 检查当前用户是否有权限操作index的文件
extern bool ValidateCurrent(int index);
// 添加用户
extern bool UserAdd(const char *name, const char *passwd, const char *parent);
// 删除用户
extern bool UserDel(const char *name);
// 显示所有用户
extern bool ShowUsers();
// 获取当前用户
extern const userEntry *GetCurrentUser();
extern bool Exist(const char *name);  // 检查用户是否存在
/* -------------------用户管理--------------------- */

/* -------------------命令------------------------- */
// 读取文件，基于Read实现，读取每个字节，中间可能会空隙
extern void ReadFile(const char *file);
// 登录
extern bool LogIn(const char *name = nullptr, const char *passwd = nullptr);
// 硬链接
extern bool Link(const char *src, const char *dst);
// 将本地文件系统的文件导入至该文件系统
extern bool Load(const char *src, const char *file);
/* -------------------命令------------------------- */

#endif  // __HEAD__