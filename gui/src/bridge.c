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

#include "cJSON.h"
#include "chat.h"
#include "paths.h"

static pid_t child_pid = -1;
static int child_in = -1;   // we write messages here
static int child_out = -1;  // we read JSON events here
static int connected = 0;

static int confirm_pending = 0;      // a dangerous tool awaits approval
static char confirm_name[128] = {0}; // its name, for the prompt

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
        if (cJSON_IsTrue(cleared)) chat_clear();
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
