#ifndef NYVD_H
#define NYVD_H

#define GET_TID 0
#define REGISTER_SERVICE 1
#define LOOKUP_SERVICE 2

#define IPC_MSG_MAX 256ULL

#define SPAWN_READ_CHUNK 2048ULL
#define SPAWN_MAX_PAGES 8ULL

typedef struct uservice {
  char *name;
  uint64_t tid;
  bool is_used;
} uservice_t;

typedef struct uservice_table {
  uservice_t **services;
  uint64_t size, cap;
} uservice_table_t;

typedef struct uprocess {
  uint64_t pid;
  uint64_t as_handle;
  uint64_t main_thread;

  char *path;
} uprocess_t;

typedef struct uproc_table {
  uprocess_t **processes;
  uint64_t size, cap;
} uproc_table_t;

void _start(void);

void get_tid(const char *path, uint64_t sender);

#endif // NYVD_H
