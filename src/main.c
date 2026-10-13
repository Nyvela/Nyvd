#include <nyvela/user/syslib.h>
#include <nyvela/mm/vmm.h>

#include "../include/nyvd.h"
#include "../include/nyvd/query.h"

uint64_t next_pid = 1;

uproc_table_t uprocess_table = {0};
uservice_table_t uservice_table = {0};

void register_service(char *path, uint64_t sender) {
  uint64_t len = ustrlen(path);
  char *name = malloc(len + 1);

  if (!name) {
    uint64_t res = 0;
    sys_ipc_send(sender, &res, sizeof(uint64_t));
    return;
  }

  umemcpy(name, path, len);
  name[len] = '\0';

  for (uint64_t i = 0; i < uservice_table.cap; i++) {
    uservice_t *service = uservice_table.services[i];

    if (service->is_used && ustrcmp(service->name, name) == 0) {
      service->tid = sender;
      
      uint64_t res = 1;
      sys_ipc_send(sender, &res, sizeof(uint64_t));
      
      free(name);

      return;
    }

    if (!service->is_used) {
      service->name = name;
 
      service->tid = sender;
      service->is_used = true;
      
      uint64_t res = 1;
      sys_ipc_send(sender, &res, sizeof(uint64_t));
      
      return;
    }
  }

  uint64_t res = 0;
  sys_ipc_send(sender, &res, sizeof(uint64_t));
  free(name);
}

void lookup_service(char *path, uint64_t sender) {
  for (uint64_t i = 0; i < uservice_table.cap; i++) {
    uservice_t *service = uservice_table.services[i];

    if (service->is_used && service->name && ustrcmp(service->name, path) == 0) {
      sys_ipc_send(sender, &service->tid, sizeof(uint64_t));
      return;
    }
  }
  
  uint64_t res = 0;
  sys_ipc_send(sender, &res, sizeof(uint64_t));
}

static bool load_program(const char *path, uint64_t *stage, uint64_t size) {
  for (uint64_t pos = 0; pos < size; pos += SPAWN_READ_CHUNK) {
    uint64_t want = size - pos;

    if (want > SPAWN_READ_CHUNK) {
      want = SPAWN_READ_CHUNK;
    }

    uint64_t buf = stage[pos / 4096] + (pos % 4096);
    int64_t got = (int64_t)sys_call(SYS_FS_READ, (uint64_t)path, buf, want, pos);

    if (got != (int64_t)want) {
      return false;
    }
  }

  return true;
}

void get_tid(const char *path, uint64_t sender) {
  for (uint64_t i = 0; i < uprocess_table.size; i++) {
    if (ustrcmp(uprocess_table.processes[i]->path, path) == 0) {
      sys_ipc_send(sender, &uprocess_table.processes[i]->main_thread, sizeof(uint64_t));
      return;
    }
  }
  
  uint64_t reply = 0;
  sys_ipc_send(sender, &reply, sizeof(uint64_t));
}

uprocess_t *spawn_process(char *path) {
  int64_t sz = (int64_t)sys_call(SYS_FS_SIZE, (uint64_t)path, 0, 0, 0);

  if (sz < 4) {
    return NULL;
  }

  uint64_t size = (uint64_t)sz;

  if (size > SPAWN_MAX_PAGES * 4096ULL) {
    return NULL;
  }

  uint64_t pages = (size + 4095) / 4096;

  uprocess_t *proc = malloc(sizeof(uprocess_t));

  if (!proc) {
    return NULL;
  }

  proc->path = path;

  if (uprocess_table.size >= uprocess_table.cap) {
    uint64_t new_cap = uprocess_table.cap * 2;
    void *tmp = realloc(uprocess_table.processes, sizeof(uprocess_t*) * new_cap);

    if (!tmp) {
      free(proc);
      return NULL;
    }

    uprocess_table.processes = tmp;
    uprocess_table.cap = new_cap;
  }

  uprocess_table.processes[uprocess_table.size++] = proc;

  uint64_t stage[SPAWN_MAX_PAGES];
  uint64_t staged = 0;
  uint64_t as = 0;
  uint64_t stack_page = 0;

  for (uint64_t i = 0; i < pages; i++) {
    stage[i] = sys_call(SYS_ALLOC_PAGE, 0, 0, 0, 0);

    if (!stage[i]) {
      goto cleanup;
    }

    staged = i + 1;
  }

  if (!load_program(path, stage, size)) {
    goto cleanup;
  }

  as = sys_call(SYS_AS_CREATE, 0, 0, 0, 0);

  if (!as) {
    goto cleanup;
  }

  stack_page = sys_call(SYS_ALLOC_PAGE, 0, 0, 0, 0);

  if (!stack_page) {
    goto cleanup;
  }

  if (!sys_call(SYS_AS_MAP, as, USER_STACK_PAGE, stack_page, 0x07)) {
    goto cleanup;
  }

  for (uint64_t i = 0; i < pages; i++) {
    if (!sys_call(SYS_AS_MAP, as, USER_CODE_VIRT + i * 4096, stage[i], 0x07)) {
      goto cleanup;
    }
  }

  staged = 0;
  stack_page = 0;

  uint64_t thread_handle = sys_call(SYS_CREATE_THREAD, USER_CODE_VIRT, as, (uint64_t)USER_STACK_PAGE + 4096, 0);

  if (!thread_handle) {
    goto cleanup;
  }

  proc->pid = next_pid++;
  proc->as_handle = as;
  proc->main_thread = thread_handle;

  return proc;

cleanup:
  for (uint64_t i = 0; i < staged; i++) {
    sys_call(SYS_FREE_PAGE, stage[i], 0, 0, 0);
  }

  if (stack_page) {
    sys_call(SYS_FREE_PAGE, stack_page, 0, 0, 0);
  }

  if (as) {
    sys_call(SYS_AS_FREE, as, 0, 0, 0);
  }
  
  uprocess_table.processes[uprocess_table.size - 1] = NULL;
  free(proc);

  return NULL;
}

void _start(void) {
  malloc_init();

  uprocess_table.size = 0;
  uprocess_table.cap = 4;
  uprocess_table.processes = malloc(sizeof(uprocess_t*) * uprocess_table.cap);

  if (!uprocess_table.processes) {
    char buf[] = "Failed to initialize process table.";
    sys_call(SYS_WRITE, 1, (uint64_t)buf, sizeof(buf), 0);
    sys_exit(1);
  }
  
  uservice_table.size = 0;
  uservice_table.cap = 32;
  uservice_table.services = malloc(sizeof(uservice_t*) * uservice_table.cap);
  
  if (!uprocess_table.processes) {
    char buf[] = "Failed to initialize service table.";
    sys_call(SYS_WRITE, 1, (uint64_t)buf, sizeof(buf), 0);
    sys_exit(1);
  }

  if (!spawn_process("/bin/sh")) {
    char buf[] = "Failed to startup /bin/sh.";
    sys_call(SYS_WRITE, 1, (uint64_t)buf, sizeof(buf), 0);
  }
  
  if (!spawn_process("/bin/npcid")) {
    char buf[] = "Failed to startup /bin/npcid.";
    sys_call(SYS_WRITE, 1, (uint64_t)buf, sizeof(buf), 0);
  }

  if (!spawn_process("/bin/nvmed")) {
    char buf[] = "Failed to startup /bin/nvmed.";
    sys_call(SYS_WRITE, 1, (uint64_t)buf, sizeof(buf), 0);
  }

  uint8_t msg[IPC_MSG_MAX];
  uint64_t sender = 0;

  for (;;) {
    int64_t size = sys_ipc_poll(msg, sizeof(msg), &sender);

    if (size < (int64_t)sizeof(ipc_nyvd_query_t)) {
      sys_yield();
      continue;
    }

    ipc_nyvd_query_t *query = (ipc_nyvd_query_t*)msg;

    if (query->path[0] == '\0') {
      uint64_t res = 0;
      sys_ipc_send(sender, &res, sizeof(uint64_t));
      continue;
    }

    switch (query->func) {
      case GET_TID:
        get_tid(query->path, sender);
        break;
      
      case REGISTER_SERVICE:
        register_service(query->path, sender);
        break;

      case LOOKUP_SERVICE:
        lookup_service(query->path, sender);
        break;

      default:
        break;
    }
  };

  sys_exit(0);
}
