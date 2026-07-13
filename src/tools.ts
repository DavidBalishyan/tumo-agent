import Anthropic from "@anthropic-ai/sdk";
import * as fs from "fs";
import * as path from "path";
import { spawnSync } from "child_process";

export const calculatorTool: Anthropic.Tool = {
    name: "calculator",
    description: "Performs basic arithmetic for exact math answers.",
    input_schema: {
        type: "object",
        properties: {
            a: { type: "number" },
            b: { type: "number" },
            operation: { type: "string", enum: ["add", "subtract", "multiply", "divide"] },
        },
        required: ["a", "b", "operation"],
    },
};

function runCalculator(input: any): string {
    const { a, b, operation } = input;
    if (operation === "add") return String(a + b);
    if (operation === "subtract") return String(a - b);
    if (operation === "multiply") return String(a * b);
    if (operation === "divide") {
        if (b === 0) return "Error: cannot divide by zero.";
        return String(a / b);
    }
    return "Error: unknown operation " + operation;
}

export const weatherTool: Anthropic.Tool = {
    name: "get_weather",
    description: "Get the current weather for a city.",
    input_schema: {
        type: "object",
        properties: { city: { type: "string" } },
        required: ["city"],
    },
};

function getWeather(input: any): string {
    const fakeData: Record<string, string> = {
        Yerevan: "28°C, sunny",
        London: "14°C, rainy",
        Tokyo: "22°C, cloudy",
    };
    return fakeData[input.city] || "20°C, clear";
}

export const dictionaryTool: Anthropic.Tool = {
    name: "dictionary",
    description: "Look up the definition of an English word.",
    input_schema: {
        type: "object",
        properties: { word: { type: "string" } },
        required: ["word"],
    },
};

function lookupWord(input: any): string {
    const defs: Record<string, string> = {
        agent: "a program that perceives, decides, and acts to reach a goal",
        tool: "a function an AI can call to take an action",
        loop: "a sequence of steps that repeats",
    };
    return defs[String(input.word).toLowerCase()] || "No definition found.";
}

// ---- Long-term memory: a JSON array of facts, on disk ----
const MEMORY_FILE = "memory.json";

export function loadMemory(): string[] {
    if (!fs.existsSync(MEMORY_FILE)) return [];
    try {
        return JSON.parse(fs.readFileSync(MEMORY_FILE, "utf-8"));
    } catch {
        return []; // unreadable file -> start fresh instead of crashing
    }
}

function saveMemory(facts: string[]): void {
    fs.writeFileSync(MEMORY_FILE, JSON.stringify(facts, null, 2));
}

export const rememberTool: Anthropic.Tool = {
    name: "remember",
    description:
        "Save a fact worth keeping about the user (name, preferences, goals) so you still know it next run.",
    input_schema: {
        type: "object",
        properties: { fact: { type: "string" } },
        required: ["fact"],
    },
};

function remember(input: any): string {
    const facts = loadMemory();
    facts.push(input.fact);
    saveMemory(facts);
    return "Saved to memory: " + input.fact;
}

// ---- File tools: read/write/list within the project directory ----
// The agent runs from the project root. We resolve every path against it and
// refuse anything that escapes it, so the agent can't wander the whole disk.
function safePath(p: string): string | null {
    const root = process.cwd();
    const resolved = path.resolve(root, p);
    if (resolved !== root && !resolved.startsWith(root + path.sep)) return null;
    return resolved;
}

export const readFileTool: Anthropic.Tool = {
    name: "read_file",
    description: "Read a UTF-8 text file, given a path relative to the project directory.",
    input_schema: {
        type: "object",
        properties: { path: { type: "string" } },
        required: ["path"],
    },
};

function readFile(input: any): string {
    const p = safePath(String(input.path ?? ""));
    if (!p) return "Error: path is outside the project directory.";
    try {
        const data = fs.readFileSync(p, "utf-8");
        return data.length > 64_000 ? data.slice(0, 64_000) + "\n... (truncated)" : data;
    } catch (e: any) {
        return "Error: " + e.message;
    }
}

export const listDirTool: Anthropic.Tool = {
    name: "list_dir",
    description: "List the entries of a directory relative to the project directory (defaults to the root).",
    input_schema: {
        type: "object",
        properties: { path: { type: "string" } },
    },
};

function listDir(input: any): string {
    const p = safePath(input.path ? String(input.path) : ".");
    if (!p) return "Error: path is outside the project directory.";
    try {
        const entries = fs.readdirSync(p, { withFileTypes: true });
        const names = entries.map((e) => (e.isDirectory() ? e.name + "/" : e.name));
        return names.length ? names.sort().join("\n") : "(empty)";
    } catch (e: any) {
        return "Error: " + e.message;
    }
}

export const writeFileTool: Anthropic.Tool = {
    name: "write_file",
    description: "Write (overwrite) a UTF-8 text file, given a path relative to the project directory.",
    input_schema: {
        type: "object",
        properties: { path: { type: "string" }, content: { type: "string" } },
        required: ["path", "content"],
    },
};

function writeFile(input: any): string {
    const p = safePath(String(input.path ?? ""));
    if (!p) return "Error: path is outside the project directory.";
    try {
        // Create any missing parent directories so writing to a new nested
        // path (e.g. notes/todo.md) just works instead of failing with ENOENT.
        fs.mkdirSync(path.dirname(p), { recursive: true });
        fs.writeFileSync(p, String(input.content ?? ""));
        return "Wrote " + input.path;
    } catch (e: any) {
        return "Error: " + e.message;
    }
}

export const runCommandTool: Anthropic.Tool = {
    name: "run_command",
    description:
        "Run a shell command in the project directory and return its output. Good for listing files, running scripts, git, and similar tasks.",
    input_schema: {
        type: "object",
        properties: { command: { type: "string" } },
        required: ["command"],
    },
};

function runShellCommand(input: any): string {
    const command = String(input.command ?? "").trim();
    if (!command) return "Error: no command given.";
    const res = spawnSync(command, {
        shell: true,
        encoding: "utf-8",
        cwd: process.cwd(),
        timeout: 15_000, // don't let a hung command wedge the agent
        maxBuffer: 1024 * 1024,
    });
    if (res.error) return "Error: " + res.error.message;

    let out = ((res.stdout || "") + (res.stderr || "")).trim();
    if (res.status !== 0) out = `(exit code ${res.status})\n` + out;
    if (!out) out = "(no output)";
    return out.length > 16_000 ? out.slice(0, 16_000) + "\n... (truncated)" : out;
}

// Tools that change something (write a file, run a command). The agent core
// asks the front-end to confirm these before running them.
export const DANGEROUS_TOOLS = new Set<string>(["write_file", "run_command"]);

export function isDangerous(name: string): boolean {
    return DANGEROUS_TOOLS.has(name);
}

// All tool definitions Claude can see
export const allTools: Anthropic.Tool[] = [
    calculatorTool,
    weatherTool,
    dictionaryTool,
    rememberTool,
    readFileTool,
    listDirTool,
    writeFileTool,
    runCommandTool,
];

// The dispatcher: maps a tool name to the real function that runs it.
// To add a tool: write its function, add it to allTools, add a case here.
export function runTool(name: string, input: any): string {
    switch (name) {
        case "calculator":
            return runCalculator(input);
        case "get_weather":
            return getWeather(input);
        case "dictionary":
            return lookupWord(input);
        case "remember":
            return remember(input);
        case "read_file":
            return readFile(input);
        case "list_dir":
            return listDir(input);
        case "write_file":
            return writeFile(input);
        case "run_command":
            return runShellCommand(input);
        default:
            return "Error: unknown tool " + name;
    }
}
