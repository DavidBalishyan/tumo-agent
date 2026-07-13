#ifndef PATHS_H
#define PATHS_H

// Work out the project layout from the executable's own path, so the app runs
// correctly no matter where it was launched from. Call once at startup.
void resolve_paths(void);

const char *project_root(void); // parent of gui/, or "" if unknown
const char *gui_dir(void);      // the gui/ directory, or "" if unknown

#endif
