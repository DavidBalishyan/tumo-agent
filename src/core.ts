import Anthropic from "@anthropic-ai/sdk";
import { allTools, runTool, loadMemory, isDangerous } from "./tools";
import { getModel, getThinkingEnabled } from "./commands";

// The headless agent core. It knows nothing about the terminal:
// instead of printing, it reports what's happening through the
// handlers below. A CLI, a TUI, or a web server can each implement
// those handlers and render however they like.
const client = new Anthropic();

// How many tokens Claude may spend thinking before it has to answer, on
// models old enough to need a fixed budget instead of adaptive thinking.
// Must stay comfortably under max_tokens, which also has to cover the reply.
const THINKING_BUDGET_TOKENS = 10000;

// Current-generation models (Sonnet 5, Opus 4.8, ...) only accept adaptive
// thinking - a fixed budget_tokens config 400s on them. Haiku 4.5 is the
// opposite: no adaptive mode, so it still needs the budget_tokens form.
// Adaptive thinking also defaults to returning empty `thinking` text unless
// display is set, so ask for the summarized form explicitly.
//
// The installed SDK's types predate both "adaptive" and "display", which are
// real, current API fields - hence the `any`.
function buildThinkingConfig(model: string): any {
    if (model.includes("haiku")) {
        return { type: "enabled", budget_tokens: THINKING_BUDGET_TOKENS };
    }
    return { type: "adaptive", display: "summarized" };
}

// ---- Customize your agent here! ----
const SYSTEM_PROMPT = `You are a helpful assistant agent with access to tools.

How to work:
1. Briefly plan your approach before acting.
2. Use tools one step at a time when they help.
3. When the user shares something worth keeping (their name, preferences,
   goals), call the remember tool.
4. When the task is done, give a clear, friendly answer.
5. For mathematical expressions, use $...$ for inline math and $$...$$ for display math. For example: $E = mc^2$ or $$\sum_{i=1}^n i = \frac{n(n+1)}{2}$$.`;

// Long-term memory is folded into the prompt on every call.
function buildSystemPrompt(): string {
    const facts = loadMemory();
    if (facts.length === 0) return SYSTEM_PROMPT;
    return SYSTEM_PROMPT + "\n\nThings you remember: " + facts.join(" ");
}

// Events the agent reports as it works. Every field is optional, so a
// front-end only implements the ones it cares about.
export type AgentHandlers = {
    onThinkingStart?: () => void;                       // Claude starts reasoning before it answers
    onThinking?: (delta: string) => void;              // a streamed chunk of that reasoning
    onThinkingEnd?: () => void;                         // reasoning is done, the reply follows
    onTextStart?: () => void;                          // first token of a reply is coming
    onText?: (delta: string) => void;                 // a streamed chunk of that reply
    onTextEnd?: () => void;                            // that reply is complete
    onToolCall?: (name: string, input: any) => void;  // Claude asked to run a tool
    onToolResult?: (name: string, result: string) => void; // what the tool returned
    onMaxTurns?: () => void;                           // gave up at the turn limit
    onTokenLimit?: (message: string) => void;          // hit a token limit (input or output)
    // Approve a tool that changes something (write_file, ...) before it runs.
    // If omitted, dangerous tools run without asking.
    confirmTool?: (name: string, input: any) => boolean | Promise<boolean>;
};

// The API reports an over-long conversation as a 400 whose body says the
// prompt exceeds the model's context window, rather than as its own error
// type. Recognize it by message so callers get a clean notice instead of an
// uncaught exception.
function isContextLengthError(e: unknown): e is InstanceType<typeof Anthropic.APIError> {
    return (
        e instanceof Anthropic.APIError &&
        e.status === 400 &&
        /prompt is too long|context length|maximum context/i.test(e.message)
    );
}

// If the conversation was left mid tool-call (a crash, a killed process, a
// session saved/loaded at the wrong moment), the last message can be an
// assistant turn with `tool_use` blocks that were never answered. The API
// rejects any request where that isn't immediately followed by matching
// `tool_result` blocks, so patch one in before we ever send it.
function repairDanglingToolUse(messages: Anthropic.MessageParam[]): void {
    const last = messages[messages.length - 1];
    if (!last || last.role !== "assistant" || typeof last.content === "string") return;

    const pending = last.content.filter(
        (b): b is Anthropic.ToolUseBlock => b.type === "tool_use"
    );
    if (pending.length === 0) return;

    const results: Anthropic.ToolResultBlockParam[] = pending.map((b) => ({
        type: "tool_result",
        tool_use_id: b.id,
        content: "Tool execution was interrupted before completion.",
        is_error: true,
    }));
    messages.push({ role: "user", content: results });
}

// ---- The agent loop (streaming, headless) ----
// Runs the think/act/observe loop, pushing each step to `handlers`. The
// `messages` array is mutated in place, so it carries the conversation
// forward across calls.
export async function runAgent(
    messages: Anthropic.MessageParam[],
    handlers: AgentHandlers = {},
    maxTurns = 10
): Promise<void> {
    repairDanglingToolUse(messages);

    let turns = 0;
    while (turns < maxTurns) {
        turns++;
        const model = getModel();
        const stream = client.messages.stream({
            model,
            max_tokens: 128000,
            system: buildSystemPrompt(),
            tools: allTools,
            messages,
            ...(getThinkingEnabled() ? { thinking: buildThinkingConfig(model) } : {}),
        } as Anthropic.MessageCreateParamsStreaming);

        // Thinking deltas arrive before the reply's text deltas, in their own
        // content block. Same started-once pattern as the text handler below.
        let thinkingStarted = false;
        stream.on("thinking", (delta) => {
            if (!thinkingStarted) {
                handlers.onThinkingStart?.();
                thinkingStarted = true;
            }
            handlers.onThinking?.(delta);
        });

        // Report each text delta as it arrives. `started` fires onTextStart
        // once, right before the first token of this turn.
        let started = false;
        stream.on("text", (delta) => {
            if (thinkingStarted) {
                handlers.onThinkingEnd?.();
                thinkingStarted = false;
            }
            if (!started) {
                handlers.onTextStart?.();
                started = true;
            }
            handlers.onText?.(delta);
        });

        let res: Anthropic.Message;
        try {
            res = await stream.finalMessage();
        } catch (e) {
            if (isContextLengthError(e)) {
                handlers.onTokenLimit?.(e.message);
                return;
            }
            throw e;
        }
        if (thinkingStarted) handlers.onThinkingEnd?.();
        if (started) handlers.onTextEnd?.();
        messages.push({ role: "assistant", content: res.content });

        if (res.stop_reason === "max_tokens") {
            handlers.onTokenLimit?.("The reply was cut off after hitting the max_tokens output limit.");
            return;
        }

        if (res.stop_reason !== "tool_use") return;

        // Run every tool Claude asked for, report it, send results back.
        // The finally block guarantees a tool_result for every tool_use block
        // (even ones we never got to), so a thrown/rejected handler can't
        // leave a dangling tool_use for the next API call to choke on.
        const toolResults: Anthropic.ToolResultBlockParam[] = [];
        try {
            for (const block of res.content) {
                if (block.type !== "tool_use") continue;
                handlers.onToolCall?.(block.name, block.input);

                let result: string;
                if (isDangerous(block.name) && handlers.confirmTool &&
                    !(await handlers.confirmTool(block.name, block.input))) {
                    result = "The user declined to run this tool.";
                } else {
                    try {
                        result = runTool(block.name, block.input);
                    } catch (e: any) {
                        result = "Tool error: " + e.message;
                    }
                }

                handlers.onToolResult?.(block.name, result);
                toolResults.push({
                    type: "tool_result",
                    tool_use_id: block.id,
                    content: result,
                });
            }
        } finally {
            const answered = new Set(toolResults.map((r) => r.tool_use_id));
            for (const block of res.content) {
                if (block.type === "tool_use" && !answered.has(block.id)) {
                    toolResults.push({
                        type: "tool_result",
                        tool_use_id: block.id,
                        content: "Tool execution was interrupted before completion.",
                        is_error: true,
                    });
                }
            }
            messages.push({ role: "user", content: toolResults });
        }
    }
    handlers.onMaxTurns?.();
}
