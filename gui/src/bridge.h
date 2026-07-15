#ifndef BRIDGE_H
#define BRIDGE_H

#include "raylib.h"

// The link to the Node agent (src/serve.ts). This module owns the child
// process and the stdio pipes, and translates the JSON event stream into
// chat-model changes.

// Change to the project root and launch the agent with its pipes wired up.
void bridge_start(void);

// Send a JSON message to the agent, e.g. bridge_send("user", "hi") or
// bridge_send("command", "/model opus").
void bridge_send(const char *type, const char *text);

// Drain any pending output, dispatching events into the chat model. Sets
// *busy to 0 when a turn finishes. A no-op once the child has exited.
void bridge_poll(int *busy);

// 1 once the agent has reported "ready"; 0 before that and after it exits.
int bridge_connected(void);

// A dangerous tool is waiting for the user's approval.
int bridge_confirm_pending(void);      // 1 while a tool awaits yes/no
const char *bridge_confirm_name(void); // the tool's name, for the prompt
void bridge_answer_confirm(int approved); // send the decision, clear the wait

// ---- LaTeX math rendering (request / response via the Node child) ----

// Result of a rendered math expression.
typedef struct {
    int id;                    // matches the request ID
    int loaded;                // 1 once the PNG data has arrived
    int w, h;                  // width and height in pixels
    int data_size;             // size of pixels buffer in bytes
    unsigned char *pixels;     // raw PNG file data (needs decoding by raylib)
} MathRenderResult;

// Request the Node process to render a LaTeX expression with a given text
// color (hex e.g. "#ffffff"). Returns an ID that can be used to poll for the
// result later.
int bridge_render_math(const char *latex, int display, Color text_color);

// Poll for a completed render result. Returns NULL if not yet available,
// or a pointer (valid until the next bridge_poll call) if the result is ready.
// The result is removed from the pending queue after one retrieval.
const MathRenderResult *bridge_math_result(int id);

// Free all pending/completed math render data.
void bridge_clear_math(void);

// Ask the agent to exit, then terminate and reap the child.
void bridge_shutdown(void);

#endif
