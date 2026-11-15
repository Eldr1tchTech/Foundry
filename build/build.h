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
#define darradddarr(darr, addition)                                            \
  darr = darradd(darr, darrlen(addition), addition)

#define darr_foreach(darr, elem)                                               \
  for (typeof(darr) elem = (darr), elem##_end = elem + darrlen(darr);          \
       elem != elem##_end; elem++)

// ==== Rebuild Logic ================================
bool is_stale(const char *src, const char *bin);
void rebuild_if_stale();

// ==== Globbing =====================================
char **glob_into(char **darr, const char *pattern);

// ==== Command Handling =============================
typedef enum OUTPUT_KIND {
  OUTPUT_KIND_EXE,
  OUTPUT_KIND_SHARED_LIB
} OUTPUT_KIND;

typedef struct project_t {
  OUTPUT_KIND kind;
  char **flags_darr;
  char **defines_darr;
  char **include_paths_darr;
  char **sources_darr;
  char **link_paths_darr;
  char **libs_darr;
  char *output;
} project_t;

char **build_command(char **darr, project_t p);
bool run_command(char **cmd);

#endif

#define FOUNDRY_BUILD_IMPLEMENTATION
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
  darr_hdr = (darr_hdr_t *)realloc(
      darr_hdr, sizeof(darr_hdr_t) + ((new_size * darr_hdr->stride)));
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

void rebuild_if_stale() {
  if (is_stale("build/build.c", "build/build")) {
    printf("Stale - Rebuilding\n");

    char **rebuild_cmd_darr = darrnew(8, sizeof(char *));
    char *rebuild_cmd[] = {"cc", "build/build.c", "-o", "build/build"};
    rebuild_cmd_darr = darradd(
        rebuild_cmd_darr, sizeof(rebuild_cmd) / sizeof(typeof(*rebuild_cmd)),
        &rebuild_cmd);

    run_command(rebuild_cmd_darr);
    darrfree(rebuild_cmd_darr);
  } else {
    printf("Current\n");
  }
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
          darr = darradd(darr, 1, &s);
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

// ==== Command Handling =============================
char **build_command(char **darr, project_t p) {
  char *a = strdup("cc");
  darr = darradd(darr, 1, &a);

  if (p.kind == OUTPUT_KIND_SHARED_LIB) {
    // Shared Lib stuff
    a = strdup("-fPIC");
    darr = darradd(darr, 1, &a);
    a = strdup("-shared");
    darr = darradd(darr, 1, &a);
  } else if (p.kind == OUTPUT_KIND_EXE) {
    if (p.flags_darr)
      darr = darradddarr(darr, p.flags_darr);
    if (p.defines_darr)
      darr = darradddarr(darr, p.defines_darr);
    if (p.include_paths_darr)
      darr = darradddarr(darr, p.include_paths_darr);
  }

  darr = darradddarr(darr, p.sources_darr);

  if (p.kind == OUTPUT_KIND_EXE) {
    if (p.link_paths_darr)
      darr = darradddarr(darr, p.link_paths_darr);
    if (p.libs_darr)
      darr = darradddarr(darr, p.libs_darr);
  }

  a = strdup("-o");
  darr = darradd(darr, 1, &a);

  darr = darradd(darr, 1, &p.output);

  return darr;
}

bool run_command(char **cmd) {
  // execvp requires a NULL-terminated argv. Copy rather than mutate the darray,
  // since it only has a trailing NULL if length < size.
  size_t n = darrlen(cmd);
  char **argv = (char **)malloc((n + 1) * sizeof(char *));
  if (!argv)
    return false;
  memcpy(argv, cmd, n * sizeof(char *));
  argv[n] = NULL;

  pid_t pid = fork();
  if (pid < 0) {
    perror("run_command - fork");
    free(argv);
    return false;
  }

  if (pid == 0) {
    // child: only returns if exec failed
    execvp(argv[0], argv);
    perror("run_command - execvp");
    _exit(127); // _exit, not exit: don't flush the parent's stdio buffers twice
  }

  free(argv);

  int status;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      perror("run_command - waitpid");
      return false;
    }
  }

  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

#endif
