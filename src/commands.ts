import Anthropic from "@anthropic-ai/sdk";
import * as os from "os";
import * as path from "path";
import { loadMemory } from "./tools";
import { saveSession, loadSession, listSessions, renderTranscript } from "./sessions";

// Terminal colors: dim for background detail, bold for the prompt.
export const dim = (s: string) => "\x1b[2m" + s + "\x1b[0m";
export const bold = (s: string) => "\x1b[1m" + s + "\x1b[0m";
export const italic = (s: string) => "\x1b[3m" + s + "\x1b[0m";

// Claude Code marks what the agent does with ⏺, and what came back with ⎿.
// Extended thinking gets its own mark (✻), the same one used on the banner.
export const DOT = "⏺";
export const ELBOW = "⎿";
export const THINK_DOT = "✻";

const BANNER = `
  ________  ____  _______
 /_  __/ / / /  |/  / __ \\
  / / / / / / /|_/ / / / /
 / / / /_/ / /  / / /_/ /
/_/  \\____/_/  /_/\\____/
`;

// Color codes take up no space on screen, so don't count them when padding.
const visibleLength = (s: string) => s.replace(/\x1b\[[0-9]+m/g, "").length;

// The startup screen: art, then a welcome box like the one Claude Code prints.
export function printBanner(): void {
    console.log(BANNER);

    const lines = [
        bold("✻ Welcome to your TUMO agent!"),
        "",
        "  " + dim("/help for commands · exit to quit"),
        "",
        "  " + dim("model:    ") + getModel(),
        "  " + dim("thinking: ") + (getThinkingEnabled() ? "on" : "off"),
        "  " + dim("cwd:      ") + process.cwd(),
    ];

    const width = Math.max(...lines.map(visibleLength)) + 2;
    console.log("╭" + "─".repeat(width) + "╮");
    for (const line of lines) {
        const pad = " ".repeat(width - visibleLength(line) - 1);
        console.log("│ " + line + pad + "│");
    }
    console.log("╰" + "─".repeat(width) + "╯");
    console.log();
}

// Models you can switch to at runtime with /model
export const MODELS: Record<string, string> = {
    sonnet: "claude-sonnet-5",
    opus: "claude-opus-4-8",
    haiku: "claude-haiku-4-5",
};

let model = MODELS.sonnet;

// agent.ts asks for the current model on every API call
export function getModel(): string {
    return model;
}

// Whether Claude shows its reasoning before answering. core.ts checks this
// on every call, so toggling mid-conversation takes effect on the next turn.
let thinkingEnabled = true;

export function getThinkingEnabled(): boolean {
    return thinkingEnabled;
}

// Run a slash command. Returns the output lines to show and whether the
// conversation was cleared. This is UI-agnostic: the CLI prints the lines,
// serve.ts forwards them as JSON. Side effects (switching model, clearing the
// messages array) happen here so every front-end shares the same behavior.
export type CommandOutcome = { lines: string[]; cleared: boolean };

export function runCommand(
    line: string,
    messages: Anthropic.MessageParam[]
): CommandOutcome {
    const [cmd, ...rest] = line.slice(1).trim().split(/\s+/);
    const arg = rest.join(" ");
    const lines: string[] = [];
    let cleared = false;

    switch (cmd) {
        case "help":
            lines.push("/model [name]      switch model (or show current)");
            lines.push("/models            list available models");
            lines.push("/thinking [on|off] toggle extended thinking (or show current)");
            lines.push("/memory            show long-term memory");
            lines.push("/cwd               show the working directory");
            lines.push("/cd <path>         change the working directory");
            lines.push("/save <name>       save this conversation");
            lines.push("/load <name>       restore a saved conversation");
            lines.push("/sessions          list saved conversations");
            lines.push("/clear             forget this conversation");
            lines.push("/exit          quit");
            break;

        case "models":
            for (const [alias, id] of Object.entries(MODELS)) {
                const mark = id === model ? "*" : " ";
                lines.push(`${mark} ${alias.padEnd(8)}${id}`);
            }
            break;

        case "model":
            if (!arg) {
                lines.push("model: " + model);
            } else {
                // an alias, or any raw model id
                const next = MODELS[arg] ?? (arg.startsWith("claude-") ? arg : null);
                if (!next) lines.push("unknown model: " + arg);
                else {
                    model = next;
                    lines.push("model -> " + model);
                }
            }
            break;

        case "thinking":
            if (!arg) {
                lines.push("thinking: " + (thinkingEnabled ? "on" : "off"));
            } else if (arg === "on" || arg === "off") {
                thinkingEnabled = arg === "on";
                lines.push("thinking -> " + arg);
            } else {
                lines.push("usage: /thinking [on|off]");
            }
            break;

        case "memory": {
            const facts = loadMemory();
            if (facts.length === 0) lines.push("(nothing remembered yet)");
            else for (const fact of facts) lines.push("- " + fact);
            break;
        }

        case "cwd":
            lines.push(process.cwd());
            break;

        case "cd": {
            // The file tools are scoped to the working directory, so this
            // re-roots what the agent can read and write.
            if (!arg) {
                lines.push("usage: /cd <path>");
                break;
            }
            const target = arg === "~" || arg.startsWith("~/")
                ? path.join(os.homedir(), arg.slice(1))
                : arg;
            try {
                process.chdir(target);
                lines.push("cwd -> " + process.cwd());
            } catch (e: any) {
                lines.push("cd failed: " + e.message);
            }
            break;
        }

        case "save":
            if (!arg) lines.push("usage: /save <name>");
            else lines.push(saveSession(arg, messages));
            break;

        case "load": {
            if (!arg) {
                lines.push("usage: /load <name>");
                break;
            }
            const res = loadSession(arg, messages);
            lines.push(res.note);
            if (res.ok) {
                // reset the view and show the conversation we just restored
                cleared = true;
                lines.push(...renderTranscript(messages));
            }
            break;
        }

        case "sessions": {
            const names = listSessions();
            if (names.length === 0) lines.push("(no saved sessions)");
            else for (const n of names) lines.push(n);
            break;
        }

        case "clear":
            messages.length = 0;
            cleared = true;
            lines.push("conversation cleared");
            break;

        default:
            lines.push(`unknown command: /${cmd} (try /help)`);
    }

    return { lines, cleared };
}

// CLI wrapper: print each line dimmed and indented, as before.
export function handleCommand(
    line: string,
    messages: Anthropic.MessageParam[]
): void {
    const { lines } = runCommand(line, messages);
    for (const l of lines) console.log(dim("  " + l));
}
