#include "chat.h"

#include <stdlib.h>
#include <string.h>

static Msg *msgs = NULL;
static int msg_count = 0, msg_cap = 0;
static int streaming_idx = -1; // assistant message currently streaming, or -1

static int msg_new(Role role) {
    if (msg_count == msg_cap) {
        msg_cap = msg_cap ? msg_cap * 2 : 16;
        msgs = realloc(msgs, msg_cap * sizeof(Msg));
    }
    Msg *m = &msgs[msg_count];
    m->role = role;
    m->cap = 64;
    m->text = malloc(m->cap);
    m->text[0] = 0;
    m->len = 0;
    return msg_count++;
}

static void msg_append(int idx, const char *s) {
    Msg *m = &msgs[idx];
    size_t add = strlen(s);
    if (m->len + add + 1 > m->cap) {
        while (m->len + add + 1 > m->cap) m->cap *= 2;
        m->text = realloc(m->text, m->cap);
    }
    memcpy(m->text + m->len, s, add + 1);
    m->len += add;
}

int chat_count(void) { return msg_count; }
const Msg *chat_get(int i) { return &msgs[i]; }

void chat_push(Role role, const char *text) {
    msg_append(msg_new(role), text);
}

void chat_begin_assistant(void) {
    streaming_idx = msg_new(ROLE_ASSISTANT);
}

void chat_begin_thinking(void) {
    streaming_idx = msg_new(ROLE_THINKING);
}

void chat_append_stream(const char *delta) {
    if (streaming_idx < 0) streaming_idx = msg_new(ROLE_ASSISTANT);
    msg_append(streaming_idx, delta);
}

void chat_end_stream(void) {
    streaming_idx = -1;
}

void chat_clear(void) {
    for (int i = 0; i < msg_count; i++) free(msgs[i].text);
    msg_count = 0;
    streaming_idx = -1;
}
