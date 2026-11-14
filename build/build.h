#ifndef FOUNDRY_BUILD_H
#define FOUNDRY_BUILD_H

#include <dirent.h>
#include <errno.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// ==== Dynamic Array ================================
typedef void *darray;

darray darrnew(size_t size, size_t stride);
void darrfree(darray darr);
size_t darrlen(darray darr);
darray darradd(darray darr, size_t item_count, void *item);

#define darr_foreach(darr, elem)                                               \
  for (typeof(darr) elem = (darr), elem##_end = elem + darrlen(darr);          \
       elem != elem##_end; elem++)

// ==== Rebuild Logic ================================
bool is_stale(const char *src, const char *bin);

// ==== Globbing =====================================
char **glob_into(char **darr, const char *pattern);

// ==== Command Handling =============================
typedef enum OUTPUT_KIND { OUTPUT_KIND_EXE } OUTPUT_KIND;

typedef struct project_t {
  OUTPUT_KIND kind;
  char **sources_darr;
  char **flags_darr;
} project_t;

char **build_command(char **darr, project_t p);
bool run_command(char **cmd);

#endif

#ifdef FOUNDRY_BUILD_IMPLEMENTATION

// ==== Dynamic Array ================================
typedef struct darr_hdr_t {
  size_t size;
  size_t length;
  size_t stride;
  char data[];
} darr_hdr_t;

darr_hdr_t *hdr_from_darr(darray darr) {
  return (darr_hdr_t *)((char *)darr - sizeof(darr_hdr_t));
}

darray darrnew(size_t size, size_t stride) {
  size_t data_size = size * stride;
  darr_hdr_t *darr_hdr = (darr_hdr_t *)malloc(sizeof(darr_hdr_t) + data_size);
  darr_hdr->stride = stride;
  darr_hdr->size = size;
  darr_hdr->length = 0;

  memset(darr_hdr->data, 0, data_size);

  return darr_hdr->data;
}

void darrfree(darray darr) { free(hdr_from_darr(darr)); }

size_t darrlen(darray darr) { return hdr_from_darr(darr)->length; }

darr_hdr_t *darr_grow(darr_hdr_t *darr_hdr, size_t new_size) {
  darr_hdr =
      realloc(darr_hdr, sizeof(darr_hdr_t) + ((new_size * darr_hdr->stride)));
  darr_hdr->size = new_size;
  memset(darr_hdr->data + (darr_hdr->stride * darr_hdr->length), 0,
         (darr_hdr->size - darr_hdr->length) * darr_hdr->stride);
  return darr_hdr;
}

darray darradd(darray darr, size_t item_count, void *item) {
  darr_hdr_t *darr_hdr = hdr_from_darr(darr);

  if (darr_hdr->length + item_count > darr_hdr->size) {
    darr_hdr = darr_grow(darr_hdr, (darr_hdr->size * 2) + item_count);
  }

  memcpy(darr_hdr->data + (darr_hdr->length * darr_hdr->stride), item,
         darr_hdr->stride * item_count);
  darr_hdr->length += item_count;
  return darr_hdr->data;
}

// ==== Rebuild Logic ================================
bool is_stale(const char *src, const char *bin) {
  struct stat s, b;

  if (stat(src, &s) == -1) {
    printf("is_stale - failed to stat src: %s", strerror(-errno));
    exit(EXIT_FAILURE);
  }
  if (stat(bin, &b) == -1) {
    printf("is_stale - failed to stat bin: %s", strerror(-errno));
    exit(EXIT_FAILURE);
  }

  if (s.st_mtim.tv_sec != b.st_mtim.tv_sec)
    return s.st_mtim.tv_sec > b.st_mtim.tv_sec;
  return s.st_mtim.tv_nsec > b.st_mtim.tv_nsec;
}

// ==== Globbing =====================================
char **collect_files_conditional(char **darr, char *dir,
                                 bool (*fn)(char *pattern)) {
  DIR *d = opendir(dir);
  if (!d) {
    printf("collect_files - failed to open directory: %s", strerror(-errno));
    exit(EXIT_FAILURE);
  }

  char path[PATH_MAX];
  struct dirent *e;
  int n;
  while ((e = readdir(d))) {
    if (e->d_name[0] == '.' &&
        (e->d_name[1] == '\0' || (e->d_name[1] == '.' && e->d_name[2] == '\0')))
      continue;

    switch (e->d_type) {
    case DT_REG:
      if (fn(e->d_name)) {
        n = snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (n < 0 || n >= sizeof(path)) {
          printf("collect_files - snprintf failed: %s", strerror(-errno));
          exit(EXIT_FAILURE);
        } else {
          char *s = strdup(path);
          darr = (char **)darradd(darr, 1, &s);
        }
      }

      break;
    case DT_DIR:
      n = snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
      if (n < 0 || n >= sizeof(path)) {
        printf("collect_files - snprintf failed: %s", strerror(-errno));
        exit(EXIT_FAILURE);
      } else {
        char *new_dir = strdup(path);
        darr = collect_files_conditional(darr, new_dir, fn);
        free(new_dir);
      }
      break;
    }
  }

  if (errno != 0) {
    printf("collect_files - error occurred: %s", strerror(-errno));
    exit(EXIT_FAILURE);
  }

  closedir(d);

  return darr;
}

// TODO: go over this
char *glob_base(const char *pattern) {
  size_t n = strcspn(pattern, "*?[");
  while (n > 0 && pattern[n - 1] != '/')
    n--;
  if (n == 0)
    return strdup(".");
  return strndup(pattern, n - 1);
}

bool dot_c_fn(char *pattern) {
  size_t len = strlen(pattern);
  if (pattern[len - 1] == 'c' && pattern[len - 2] == '.') {
    return true;
  } else {
    return false;
  }
}

char **glob_into(char **darr, const char *pattern) {
  // collect candidate files
  char *base = glob_base(pattern);
  darr = collect_files_conditional(darr, base, dot_c_fn);
  free(base);
  return darr;
}

#endif
