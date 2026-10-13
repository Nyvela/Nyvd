#ifndef NYVDQUERY_H
#define NYVDQUERY_H

#define GET_TID 0
#define REGISTER_SERVICE 1
#define LOOKUP_SERVICE 2

#define NYVD_PATH_MAX 64

typedef struct ipc_nyvd_query {
  uint8_t func;
  char path[NYVD_PATH_MAX];
} ipc_nyvd_query_t;

#endif // NYVDQUERY_H
