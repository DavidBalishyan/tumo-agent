#define _POSIX_C_SOURCE 200809L
#include "paths.h"

#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

static char g_root[PATH_MAX];    // project root (parent of gui/)
static char g_gui_dir[PATH_MAX]; // the gui/ directory itself

void resolve_paths(void) {
    g_root[0] = 0;
    g_gui_dir[0] = 0;
    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n <= 0) return;
    exe[n] = 0;
    char b1[PATH_MAX], b2[PATH_MAX];
    strncpy(b1, exe, sizeof(b1) - 1); b1[sizeof(b1) - 1] = 0;
    char *guidir = dirname(b1);                 // .../gui
    strncpy(g_gui_dir, guidir, sizeof(g_gui_dir) - 1);
    g_gui_dir[sizeof(g_gui_dir) - 1] = 0;
    strncpy(b2, guidir, sizeof(b2) - 1); b2[sizeof(b2) - 1] = 0;
    char *root = dirname(b2);                    // project root
    strncpy(g_root, root, sizeof(g_root) - 1);
    g_root[sizeof(g_root) - 1] = 0;
}

const char *project_root(void) { return g_root; }
const char *gui_dir(void) { return g_gui_dir; }
