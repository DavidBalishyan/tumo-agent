import Anthropic from "@anthropic-ai/sdk";
import { loadMemory } from "./tools";

// ============================================================
// Slash commands for the chat loop — the /help, /model, /clear
// you type at the "> " prompt. Add your own in the switch below.
// ============================================================

// Terminal colors: dim for background detail, bold for the prompt.
export const dim = (s: string) => "\x1b[2m" + s + "\x1b[0m";
export const bold = (s: string) => "\x1b[1m" + s + "\x1b[0m";

// Claude Code marks what the agent does with ⏺, and what came back with ⎿.
export const DOT = "⏺";
export const ELBOW = "⎿";

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
    "  " + dim("model: ") + getModel(),
    "  " + dim("cwd:   ") + process.cwd(),
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
  sonnet: "claude-sonnet-4-6",
  opus: "claude-opus-4-8",
  haiku: "claude-haiku-4-5",
};

let model = MODELS.sonnet;

// agent.ts asks for the current model on every API call
export function getModel(): string {
  return model;
}

export function handleCommand(
  line: string,
  messages: Anthropic.MessageParam[]
): void {
  const [cmd, ...rest] = line.slice(1).trim().split(/\s+/);
  const arg = rest.join(" ");

  switch (cmd) {
    case "help":
      console.log(dim("  /model [name]  switch model (or show current)"));
      console.log(dim("  /models        list available models"));
      console.log(dim("  /memory        show long-term memory"));
      console.log(dim("  /clear         forget this conversation"));
      console.log(dim("  /exit          quit"));
      return;

    case "models":
      for (const [alias, id] of Object.entries(MODELS)) {
        const mark = id === model ? "*" : " ";
        console.log(dim(`  ${mark} ${alias.padEnd(8)}${id}`));
      }
      return;

    case "model": {
      if (!arg) return console.log(dim("  model: " + model));
      // an alias, or any raw model id
      const next = MODELS[arg] ?? (arg.startsWith("claude-") ? arg : null);
      if (!next) return console.log(dim("  unknown model: " + arg));
      model = next;
      console.log(dim("  model -> " + model));
      return;
    }

    case "memory": {
      const facts = loadMemory();
      if (facts.length === 0) return console.log(dim("  (nothing remembered yet)"));
      for (const fact of facts) console.log(dim("  - " + fact));
      return;
    }

    case "clear":
      messages.length = 0;
      console.log(dim("  conversation cleared"));
      return;

    default:
      console.log(dim(`  unknown command: /${cmd} (try /help)`));
  }
}
