#define FOUNDRY_BUILD_IMPLEMENTATION
#include "build.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  printf("is_stale: %i\n", is_stale("build/build.c", "build/build"));

  if (argc > 1) {
    if (strcmp(argv[1], "clean") == 0) {
    }
  }

  // Flags
  char **flags = darrnew(16, sizeof(char *));
  char *f[2] = {"-Wall", "-Werror"};
  darradd(flags, 2, &f);

  // Sources
  char **srcs = darrnew(16, sizeof(char *));
  srcs = glob_into(srcs, "core/**/*.c");

  project_t p = {.kind = OUTPUT_KIND_SHARED_LIB,
                 .sources_darr = srcs,
                 .flags_darr = flags,
                 .output = strdup("build/bin/libfoundry.so")};

  char **cmd = build_command(darrnew(16, sizeof(char *)), p);

  darr_foreach(cmd, elem) { printf("%s\n", *elem); }

  run_command(cmd);

  darrfree(cmd);

  return 0;
}