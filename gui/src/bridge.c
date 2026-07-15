#define _POSIX_C_SOURCE 200809L
#include "bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <math.h>

#include "cJSON.h"
#include "chat.h"
#include "paths.h"

static pid_t child_pid = -1;
static int child_in = -1;   // we write messages here
static int child_out = -1;  // we read JSON events here
static int connected = 0;

static int confirm_pending = 0;      // a dangerous tool awaits approval
static char confirm_name[128] = {0}; // its name, for the prompt

// ---- LaTeX math rendering ----
#define MAX_MATH_RESULTS 256
static MathRenderResult math_results[MAX_MATH_RESULTS];
static int math_result_count = 0;
static int math_next_id = 1;

int bridge_render_math(const char *latex, int display, Color text_color) {
    if (child_in < 0) return 0;
    int id = math_next_id++;

    // Store a pending entry
    if (math_result_count < MAX_MATH_RESULTS) {
        MathRenderResult *r = &math_results[math_result_count++];
        r->id = id;
        r->loaded = 0;
        r->w = r->h = 0;
        r->pixels = NULL;
    }

    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "render_math");
    cJSON_AddNumberToObject(o, "id", id);
    cJSON_AddStringToObject(o, "latex", latex);
    cJSON_AddBoolToObject(o, "display", display ? 1 : 0);
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x", text_color.r, text_color.g, text_color.b);
    cJSON_AddStringToObject(o, "color", hex);
    char *s = cJSON_PrintUnformatted(o);
    if (s) {
        (void)!write(child_in, s, strlen(s));
        (void)!write(child_in, "\n", 1);
        free(s);
    }
    cJSON_Delete(o);
    return id;
}

const MathRenderResult *bridge_math_result(int id) {
    for (int i = 0; i < math_result_count; i++) {
        if (math_results[i].id == id && math_results[i].loaded) {
            return &math_results[i];
        }
    }
    return NULL;
}

void bridge_clear_math(void) {
    for (int i = 0; i < math_result_count; i++) {
        free(math_results[i].pixels);
        math_results[i].pixels = NULL;
    }
    math_result_count = 0;
}

// Decode a base64 string into a byte buffer. Returns malloc'd data or NULL.
// Handles standard base64 with optional padding.
static unsigned char *base64_decode(const char *s, int *out_len) {
    if (!s || !*s) { *out_len = 0; return NULL; }
    int len = (int)strlen(s);
    // Each 4 base64 chars -> 3 bytes.
    int est = len / 4 * 3;
    // Adjust for padding
    if (len >= 2 && s[len - 2] == '=') est -= 2;
    else if (len >= 1 && s[len - 1] == '=') est -= 1;

    // Initialize the base64 decode table once:
    static int tbl_ok = 0;
    static unsigned char tbl[256];
    if (!tbl_ok) {
        for (int i = 0; i < 256; i++) tbl[i] = 0;
        for (int c = 'A'; c <= 'Z'; c++) tbl[c] = (unsigned char)(c - 'A');
        for (int c = 'a'; c <= 'z'; c++) tbl[c] = (unsigned char)(c - 'a' + 26);
        for (int c = '0'; c <= '9'; c++) tbl[c] = (unsigned char)(c - '0' + 52);
        tbl['+'] = 62; tbl['/'] = 63;
        tbl_ok = 1;
    }

    unsigned char *out = malloc((size_t)est + 4);
    if (!out) { *out_len = 0; return NULL; }
    int o = 0;
    unsigned char buf[4];
    int bi = 0;
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '=') break;
        if (c > 127 || (tbl[c] == 0 && c != 'A')) continue; // skip whitespace etc.
        buf[bi++] = tbl[c];
        if (bi == 4) {
            out[o++] = (unsigned char)((buf[0] << 2) | (buf[1] >> 4));
            out[o++] = (unsigned char)((buf[1] << 4) | (buf[2] >> 2));
            out[o++] = (unsigned char)((buf[2] << 6) | buf[3]);
            bi = 0;
        }
    }
    if (bi >= 2) out[o++] = (unsigned char)((buf[0] << 2) | (buf[1] >> 4));
    if (bi >= 3) out[o++] = (unsigned char)((buf[1] << 4) | (buf[2] >> 2));
    *out_len = o;
    return out;
}

void bridge_start(void) {
    // Run from the project root so .env, memory.json, and src/serve.ts resolve.
    const char *root = project_root();
    if (root[0] && chdir(root) != 0) perror("chdir");

    int inpipe[2], outpipe[2];
    if (pipe(inpipe) || pipe(outpipe)) { perror("pipe"); return; }

    child_pid = fork();
    if (child_pid < 0) { perror("fork"); return; }

    if (child_pid == 0) {
        // child: wire pipes to stdin/stdout, keep stderr for diagnostics
        dup2(inpipe[0], STDIN_FILENO);
        dup2(outpipe[1], STDOUT_FILENO);
        close(inpipe[0]); close(inpipe[1]);
        close(outpipe[0]); close(outpipe[1]);
        const char *cmd = getenv("AGENT_CMD");
        if (!cmd) cmd = "exec node_modules/.bin/tsx src/serve.ts";
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127); // exec failed
    }

    // parent
    close(inpipe[0]);
    close(outpipe[1]);
    child_in = inpipe[1];
    child_out = outpipe[0];
    fcntl(child_out, F_SETFL, O_NONBLOCK); // never block the render loop
}

void bridge_send(const char *type, const char *text) {
    if (child_in < 0) return;
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", type);
    cJSON_AddStringToObject(o, "text", text);
    char *s = cJSON_PrintUnformatted(o);
    if (s) {
        (void)!write(child_in, s, strlen(s));
        (void)!write(child_in, "\n", 1);
        free(s);
    }
    cJSON_Delete(o);
}

int bridge_connected(void) { return connected; }

int bridge_confirm_pending(void) { return confirm_pending; }
const char *bridge_confirm_name(void) { return confirm_name; }

void bridge_answer_confirm(int approved) {
    confirm_pending = 0;
    if (child_in < 0) return;
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "confirm_response");
    cJSON_AddBoolToObject(o, "approved", approved ? 1 : 0);
    char *s = cJSON_PrintUnformatted(o);
    if (s) {
        (void)!write(child_in, s, strlen(s));
        (void)!write(child_in, "\n", 1);
        free(s);
    }
    cJSON_Delete(o);
}

// Turn one JSON event into chat-model changes.
static void handle_event(const char *type, cJSON *o) {
    if (strcmp(type, "ready") == 0) {
        connected = 1;
    } else if (strcmp(type, "thinking_start") == 0) {
        chat_begin_thinking();
    } else if (strcmp(type, "thinking") == 0) {
        cJSON *d = cJSON_GetObjectItem(o, "delta");
        if (cJSON_IsString(d)) chat_append_stream(d->valuestring);
    } else if (strcmp(type, "thinking_end") == 0) {
        chat_end_stream();
    } else if (strcmp(type, "text_start") == 0) {
        chat_begin_assistant();
    } else if (strcmp(type, "text") == 0) {
        cJSON *d = cJSON_GetObjectItem(o, "delta");
        if (cJSON_IsString(d)) chat_append_stream(d->valuestring);
    } else if (strcmp(type, "text_end") == 0) {
        chat_end_stream();
    } else if (strcmp(type, "tool_call") == 0) {
        cJSON *name = cJSON_GetObjectItem(o, "name");
        cJSON *input = cJSON_GetObjectItem(o, "input");
        char *args = input ? cJSON_PrintUnformatted(input) : NULL;
        char buf[1024];
        snprintf(buf, sizeof(buf), "> %s(%s)",
                 cJSON_IsString(name) ? name->valuestring : "?",
                 args ? args : "");
        chat_push(ROLE_TOOL, buf);
        if (args) free(args);
    } else if (strcmp(type, "tool_result") == 0) {
        cJSON *result = cJSON_GetObjectItem(o, "result");
        char buf[1024];
        snprintf(buf, sizeof(buf), "  = %s",
                 cJSON_IsString(result) ? result->valuestring : "");
        chat_push(ROLE_TOOL, buf);
    } else if (strcmp(type, "max_turns") == 0) {
        chat_push(ROLE_SYSTEM, "[stopped: hit the max turn limit]");
    } else if (strcmp(type, "error") == 0) {
        cJSON *m = cJSON_GetObjectItem(o, "message");
        char buf[1024];
        snprintf(buf, sizeof(buf), "[error] %s",
                 cJSON_IsString(m) ? m->valuestring : "unknown");
        chat_push(ROLE_SYSTEM, buf);
    } else if (strcmp(type, "command_result") == 0) {
        cJSON *cleared = cJSON_GetObjectItem(o, "cleared");
        if (cJSON_IsTrue(cleared)) { chat_clear(); bridge_clear_math(); }
        cJSON *lines = cJSON_GetObjectItem(o, "lines");
        if (cJSON_IsArray(lines)) {
            int n = cJSON_GetArraySize(lines);
            for (int i = 0; i < n; i++) {
                cJSON *l = cJSON_GetArrayItem(lines, i);
                if (cJSON_IsString(l)) chat_push(ROLE_SYSTEM, l->valuestring);
            }
        }
    } else if (strcmp(type, "confirm") == 0) {
        // a dangerous tool is waiting on the user; the tool_call line above
        // already shows the details, so we just need the name for the prompt
        cJSON *name = cJSON_GetObjectItem(o, "name");
        snprintf(confirm_name, sizeof(confirm_name), "%s",
                 cJSON_IsString(name) ? name->valuestring : "this tool");
        confirm_pending = 1;
    } else if (strcmp(type, "math_rendered") == 0) {
        cJSON *id = cJSON_GetObjectItem(o, "id");
        cJSON *w = cJSON_GetObjectItem(o, "width");
        cJSON *h = cJSON_GetObjectItem(o, "height");
        cJSON *data = cJSON_GetObjectItem(o, "data");
        if (cJSON_IsNumber(id) && cJSON_IsNumber(w) && cJSON_IsNumber(h) && cJSON_IsString(data)) {
            int rid = id->valueint;
            int rw = w->valueint, rh = h->valueint;
            int png_len;
            unsigned char *png_data = base64_decode(data->valuestring, &png_len);
            if (png_data) {
                // Find or create a result slot
                MathRenderResult *rr = NULL;
                for (int i = 0; i < math_result_count; i++) {
                    if (math_results[i].id == rid) { rr = &math_results[i]; break; }
                }
                if (!rr && math_result_count < MAX_MATH_RESULTS) {
                    rr = &math_results[math_result_count++];
                    rr->id = rid;
                }
                if (rr) {
                    free(rr->pixels);
                    rr->w = rw;
                    rr->h = rh;
                    rr->data_size = png_len;
                    rr->pixels = png_data;
                    rr->loaded = 1;
                } else {
                    free(png_data);
                }
            }
        }
    }
    // "turn_end" is handled in bridge_poll so it can clear the busy flag.
}

void bridge_poll(int *busy) {
    static char acc[1 << 16];
    static size_t acc_len = 0;

    if (child_out < 0) return;

    char tmp[4096];
    for (;;) {
        ssize_t r = read(child_out, tmp, sizeof(tmp));
        if (r > 0) {
            if (acc_len + r < sizeof(acc)) {
                memcpy(acc + acc_len, tmp, r);
                acc_len += r;
            }
            // process every complete line in the accumulator
            size_t start = 0;
            for (size_t i = 0; i < acc_len; i++) {
                if (acc[i] != '\n') continue;
                acc[i] = 0;
                char *line = acc + start;
                if (*line) {
                    cJSON *o = cJSON_Parse(line);
                    if (o) {
                        cJSON *t = cJSON_GetObjectItem(o, "type");
                        if (cJSON_IsString(t)) {
                            if (strcmp(t->valuestring, "turn_end") == 0) *busy = 0;
                            else handle_event(t->valuestring, o);
                        }
                        cJSON_Delete(o);
                    }
                }
                start = i + 1;
            }
            memmove(acc, acc + start, acc_len - start);
            acc_len -= start;
        } else if (r == 0) {
            // child closed stdout: it has exited
            if (connected) chat_push(ROLE_SYSTEM, "[agent process ended]");
            connected = 0;
            close(child_out);
            child_out = -1;
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            if (errno == EINTR) continue;
            break;
        }
    }
}

void bridge_shutdown(void) {
    if (child_in >= 0) {
        (void)!write(child_in, "{\"type\":\"exit\"}\n", 16);
        close(child_in);
        child_in = -1;
    }
    if (child_pid > 0) {
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        child_pid = -1;
    }
}
