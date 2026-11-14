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

  char** srcs = darrnew(16, sizeof(char*));
  srcs = glob_into(srcs, "core/**/*.c");

  darr_foreach(srcs, elem) {
    printf("%s\n", elem);
  }

  project_t p = {.kind = OUTPUT_KIND_EXE,
                 .sources_darr = srcs,
                 .flags_darr = darradd(darrnew(16, sizeof(char *)), 2,
                                  (char *[]){"-Wall", "-Werror"})};

  char **cmd = build_command(darrnew(16, sizeof(char*)), p);
  run_command(cmd);
  darrfree(cmd);

  return 0;
}