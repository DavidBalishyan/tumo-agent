import Anthropic from "@anthropic-ai/sdk";
import * as fs from "fs";
import * as path from "path";

// ============================================================
// Save and resume conversations. A session is just the `messages`
// array written to sessions/<name>.json. Loading replaces the live
// array in place, so the agent picks the chat back up with full
// context. Used by the /save, /load, and /sessions commands.
// ============================================================

const SESSIONS_DIR = "sessions";

// Keep names to a safe, flat set so a name can't escape the directory.
function validName(name: string): boolean {
    return /^[A-Za-z0-9_-]+$/.test(name);
}

function sessionFile(name: string): string {
    return path.join(SESSIONS_DIR, name + ".json");
}

export function saveSession(name: string, messages: Anthropic.MessageParam[]): string {
    if (!validName(name)) return "bad name: use letters, numbers, - and _ only";
    try {
        fs.mkdirSync(SESSIONS_DIR, { recursive: true });
        fs.writeFileSync(sessionFile(name), JSON.stringify(messages, null, 2));
        return `saved ${messages.length} messages to '${name}'`;
    } catch (e: any) {
        return "save failed: " + e.message;
    }
}

// Replaces the contents of `messages` in place. Returns whether it worked plus
// a status note for the user.
export function loadSession(
    name: string,
    messages: Anthropic.MessageParam[]
): { ok: boolean; note: string } {
    if (!validName(name)) return { ok: false, note: "bad name: use letters, numbers, - and _ only" };
    try {
        const loaded = JSON.parse(fs.readFileSync(sessionFile(name), "utf-8"));
        messages.length = 0;
        messages.push(...loaded);
        return { ok: true, note: `loaded ${loaded.length} messages from '${name}'` };
    } catch (e: any) {
        if (e.code === "ENOENT") return { ok: false, note: `no session named '${name}'` };
        return { ok: false, note: "load failed: " + e.message };
    }
}

export function listSessions(): string[] {
    try {
        return fs
            .readdirSync(SESSIONS_DIR)
            .filter((f) => f.endsWith(".json"))
            .map((f) => f.slice(0, -".json".length))
            .sort();
    } catch {
        return [];
    }
}

// A plain-text rendering of a conversation, so /load can show what came back.
export function renderTranscript(messages: Anthropic.MessageParam[]): string[] {
    const lines: string[] = [];
    for (const m of messages) {
        if (typeof m.content === "string") {
            lines.push(`${m.role}: ${m.content}`);
            continue;
        }
        for (const block of m.content) {
            const b: any = block;
            if (b.type === "text") lines.push(`${m.role}: ${b.text}`);
            else if (b.type === "tool_use") lines.push(`${m.role}: [tool ${b.name}]`);
            else if (b.type === "tool_result") {
                const c = typeof b.content === "string" ? b.content : "[result]";
                lines.push(`tool: ${c}`);
            }
        }
    }
    return lines;
}
