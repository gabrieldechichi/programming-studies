#include "platform.h"

// Async file loading functions
void *load_file_thread(void *arg) {
  file_handle_t *handle = (file_handle_t *)arg;

  FILE *file = fopen(handle->path, "r");
  if (!file) {
    pthread_mutex_lock(&handle->mutex);
    handle->state = LOADING_ERROR;
    pthread_mutex_unlock(&handle->mutex);
    return NULL;
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Allocate buffer and read content
  char *content = (char *)malloc((size_t)size + 1);
  if (!content) {
    fclose(file);
    pthread_mutex_lock(&handle->mutex);
    handle->state = LOADING_ERROR;
    pthread_mutex_unlock(&handle->mutex);
    return NULL;
  }

  size_t read_size = fread(content, 1, (size_t)size, file);
  content[read_size] = '\0';
  fclose(file);

  // Update handle with loaded data
  pthread_mutex_lock(&handle->mutex);
  handle->content = content;
  handle->size = read_size;
  handle->state = LOADING_COMPLETE;
  pthread_mutex_unlock(&handle->mutex);

  return NULL;
}

file_handle_t *load_file_async(const char *path) {
  file_handle_t *handle = (file_handle_t *)malloc(sizeof(file_handle_t));
  if (!handle)
    return NULL;

  handle->path = strdup(path);
  handle->content = NULL;
  handle->size = 0;
  handle->state = LOADING_PENDING;
  pthread_mutex_init(&handle->mutex, NULL);

  if (pthread_create(&handle->thread, NULL, load_file_thread, handle) != 0) {
    free(handle->path);
    free(handle);
    return NULL;
  }

  return handle;
}

bool is_file_ready(file_handle_t *handle) {
  if (!handle)
    return false;

  pthread_mutex_lock(&handle->mutex);
  bool ready = (handle->state != LOADING_PENDING);
  pthread_mutex_unlock(&handle->mutex);

  return ready;
}

const char *get_file_content(file_handle_t *handle) {
  if (!handle)
    return NULL;

  pthread_mutex_lock(&handle->mutex);
  const char *content = handle->content;
  pthread_mutex_unlock(&handle->mutex);

  return content;
}

void free_file_handle(file_handle_t *handle) {
  if (!handle)
    return;

  pthread_join(handle->thread, NULL);
  pthread_mutex_destroy(&handle->mutex);
  free(handle->path);
  free(handle->content);
  free(handle);
}
