#include <stdlib.h>
#define FOUNDRY_BUILD_IMPLEMENTATION
#include "build.h"

#include <stdio.h>
#include <string.h>

void build_core_lib() {
  char **srcs = darrnew(16, sizeof(char *));
  srcs = glob_into(srcs, "core/**/*.c");

  project_t p = {.kind = OUTPUT_KIND_SHARED_LIB,
                 .sources_darr = srcs,
                 .output = strdup("build/bin/libfoundry.so")};

  char **cmd = build_command(darrnew(16, sizeof(char *)), p);
  run_command(cmd);

  darrfree(cmd);
  darrfree(srcs);
  free(p.output);
}

void build_tests() {
  char **flags = darrnew(4, sizeof(char *));
  char *f[2] = {"-Wall", "-Werror"};
  darradd(flags, sizeof(f) / sizeof(typeof(*f)), &f);

  // Sources
  char **srcs = darrnew(16, sizeof(char *));
  srcs = glob_into(srcs, "tests/**/*.c");

  // Include paths
  char **include_paths = darrnew(4, sizeof(char *));
  char *ip[1] = {"-Icore"};
  darradd(include_paths, sizeof(ip) / sizeof(typeof(*ip)), &ip);

  // Link paths
  char **link_paths = darrnew(4, sizeof(char *));
  char *lp[1] = {"-Lbuild/bin"};
  darradd(link_paths, sizeof(lp) / sizeof(typeof(*lp)), &lp);

  // Libs
  char **libs = darrnew(4, sizeof(char *));
  char *l[1] = {"-lfoundry"};
  darradd(libs, sizeof(l) / sizeof(typeof(*l)), &l);

  project_t p = {.kind = OUTPUT_KIND_EXE,
                 .sources_darr = srcs,
                 .flags_darr = flags,
                 .include_paths_darr = include_paths,
                 .link_paths_darr = link_paths,
                 .libs_darr = libs,
                 .output = strdup("build/bin/foundrytests")};

  char **cmd = build_command(darrnew(16, sizeof(char *)), p);
  run_command(cmd);

  darrfree(cmd);
  darrfree(flags);
  darrfree(srcs);
  free(p.output);
}

void project_run() {}

int main(int argc, char **argv) {
  rebuild_if_stale();

  if (argc > 1) {
    if (strcmp(argv[1], "clean") == 0) {
      // TODO: Needed for incremental builds
    }
  } else {
    printf("Building Library...\n");
    build_core_lib();

    printf("Building Tests...\n");
    build_tests();

    printf("Running...\n");
    project_run();
  }

  return 0;
}