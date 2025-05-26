#ifndef __HEAD__
#define __HEAD__
#include <semaphore.h>
#include <stdbool.h>
#include <atomic>
#include <set>
#include <string>

constexpr int MAX_NAME_LENGTH = 32;
constexpr int MAX_PASSWD_LENGTH = 32;
constexpr int INODE_SIZE = 128;
constexpr int BLOCK_SIZE = 4096;                          // 数据块内容大小
constexpr int DISK_SIZE = 50 * 1024 * 1024 + BLOCK_SIZE;  // 50MB 磁盘
constexpr int MEM_SIZE = DISK_SIZE;  // 因为没有缓冲池，为了方便暂时内存 == 磁盘大小。

constexpr int MAX_BLOCK_NUMBER = DISK_SIZE / (INODE_SIZE + BLOCK_SIZE);
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
  int first_index[MAX_FIRST_INDEX];  // 数据域。
} inode;

static_assert(BLOCK_SIZE % INODE_SIZE == 0);
static_assert(sizeof(inode) == 128);

typedef struct dataBlock  // 数据块
{
  char content[BLOCK_SIZE];
} dataBlock;

typedef struct indexBlock {
  int data_block[(BLOCK_SIZE) / sizeof(int)];
} indexBlock;

typedef struct superBlock {
  int user_info_id;  // 用户信息的节点。
  int root_dir_id;   // 根目录的节点。
  int stack_num;     // 空闲栈
  int stack[(BLOCK_SIZE - sizeof(root_dir_id) - sizeof(stack_num) - sizeof(user_info_id)) /
            sizeof(int)];
} superBlock;

typedef struct freeBlock {
  const int _;
  const int __;  // 为了对齐superBlock;
  int stack_num;
  int stack[(BLOCK_SIZE - sizeof(_) - sizeof(__) - sizeof(stack_num)) / sizeof(int)];
  ;  // stack[0] 是成组链接的下一组。 stack[1] 指向本身。
} freeBlock;

typedef struct dirEntry {
  int file_id;
} dirEntry;

typedef struct userEntry {
  char user_name[MAX_NAME_LENGTH];
  char user_passwd[MAX_PASSWD_LENGTH];
  char parent[MAX_NAME_LENGTH];
} userEntry;

typedef struct context {
  std::atomic<bool> flag;
  sem_t mutex;
} context;

/* -------------------全局变量--------------------- */
extern int current_dir_index;
extern std::set<int> open_file;
extern userEntry user_info;
extern int fd;
extern char *memory;  // shared_memory
/* -------------------全局变量--------------------- */

/* -------------------磁盘操作--------------------- */
extern bool FormatFileSystem(const char *file);
extern bool OpenFileSystem(const char *file);
extern bool CloseFileSystem();
extern superBlock *GetSuperBlock();
extern inode *GetInode(int index);
extern dataBlock *GetBlock(int index);
extern indexBlock *GetIndexBlock(int index);
extern void PutBlock(int index, bool write);
extern void PutInode(int index, bool write);
extern void PutSuperBlock(bool write);
extern int AllocDataBlock();
extern void ReleaseDataBlock(int index);
// extern void FlushDisk();
/* -------------------磁盘操作--------------------- */

/* -------------------文件操作--------------------- */
extern int Write(int index, int pos, int len, const char *buf);
extern int Append(int index, int len, const char *buf);
extern int Read(int index, int pos, int len, char *buf);
extern int ReadEntry(int index, int pos, int size, char *buf);
extern int WriteEntry(int index, int pos, int size, const char *buf);
extern int NewFile(file_type type, const char *file_name, const char *owner_name);
extern bool RemoveFile(int index);
/* -------------------文件操作--------------------- */

/* -------------------文件夹操作------------------- */
extern bool CreateFile(const char *file_name);
extern int Open(const char *file_name, int *pos = nullptr);
extern int OpenFile(const char *file_name);
extern bool CloseFile(const char *file_name);
extern bool DeleteFile(const char *file_name);
extern bool DeleteDir(const char *dir_name);
extern bool CreateDir(const char *dir_name);
extern bool NextDir(const char *dir_name);
extern bool ReadDir(int index, int *len, int *files);
extern void ShowDir();
extern bool LastDir();
extern const char *NowDir();
extern std::string GetPath();
extern bool Rename(const char *old_name, const char *new_name);
extern bool Move(const char *file, const char *dir);
extern bool Copy(const char *file, const char *dir);
extern bool IsOpen(int fd);
/* -------------------文件夹操作------------------- */

/* -------------------用户管理--------------------- */
extern bool Validate(const char *owner, const char *worker);
extern bool ValidateCurrent(int index);
extern bool UserAdd(const char *name, const char *passwd, const char *parent);
extern bool UserDel(const char *name);
extern bool ShowUsers();
extern const userEntry *GetCurrentUser();
/* -------------------用户管理--------------------- */

/* -------------------命令------------------------- */
extern void ReadFile(const char *file);
extern bool LogIn(const char *name = nullptr, const char *passwd = nullptr);
extern bool Link(const char *src, const char *dst);
extern bool Load(const char *src, const char *file);
/* -------------------命令------------------------- */

#endif  // __HEAD__