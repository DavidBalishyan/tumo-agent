#ifndef BRIDGE_H
#define BRIDGE_H

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

// Ask the agent to exit, then terminate and reap the child.
void bridge_shutdown(void);

#endif
