#ifndef H_PLATFORM
#define H_PLATFORM

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Async file loading system
typedef enum {
  LOADING_PENDING,
  LOADING_COMPLETE,
  LOADING_ERROR
} loading_state_t;

typedef struct {
  char *path;
  char *content;
  size_t size;
  loading_state_t state;
  pthread_t thread;
  pthread_mutex_t mutex;
} file_handle_t;

void *load_file_thread(void *arg);
file_handle_t *load_file_async(const char *path);
bool is_file_ready(file_handle_t *handle);
const char *get_file_content(file_handle_t *handle);
void free_file_handle(file_handle_t *handle);

#endif // H_PLATFORM
