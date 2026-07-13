#ifndef CHAT_H
#define CHAT_H

#include <stddef.h>

// The chat transcript: an ordered list of messages the UI renders. This module
// owns the storage; other modules touch it only through these functions.

typedef enum { ROLE_USER, ROLE_ASSISTANT, ROLE_TOOL, ROLE_SYSTEM, ROLE_THINKING } Role;

typedef struct {
    Role role;
    char *text;
    size_t len, cap;
} Msg;

// Read access, for the renderer.
int chat_count(void);
const Msg *chat_get(int i);

// Append a complete message.
void chat_push(Role role, const char *text);

// A streaming assistant reply: start one, append deltas, then finish.
void chat_begin_assistant(void);
void chat_append_stream(const char *delta);
void chat_end_stream(void);

// Same idea, for the reasoning Claude streams before it starts answering.
void chat_begin_thinking(void);

// Drop every message.
void chat_clear(void);

#endif
