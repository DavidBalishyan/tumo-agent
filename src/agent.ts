import "dotenv/config";
import Anthropic from "@anthropic-ai/sdk";
import * as readline from "readline/promises";
import { runAgent, AgentHandlers } from "./core";
import { dim, bold, italic, DOT, ELBOW, THINK_DOT, handleCommand, printBanner } from "./commands";

// The CLI front-end. This is the only file that touches
// stdio: it turns the agent core's events (core.ts) back into
// stdout. Swap it for an Ink TUI or a web server and the core
// stays exactly the same.
let rl: readline.Interface;

// Map each agent event to terminal output, keeping the ⏺ / ⎿ style.
const cli: AgentHandlers = {
    onThinkingStart: () => process.stdout.write(dim(italic(`\n${THINK_DOT} Thinking…\n`))),
    onThinking: (delta) => process.stdout.write(dim(italic(delta))),
    onThinkingEnd: () => process.stdout.write("\n"),
    onTextStart: () => process.stdout.write(`\n${DOT} `),
    onText: (delta) => process.stdout.write(delta),
    onTextEnd: () => process.stdout.write("\n"),
    onToolCall: (name, input) =>
        console.log(`\n${DOT} ${bold(name)}(${dim(JSON.stringify(input))})`),
    onToolResult: (_name, result) => console.log(dim(`  ${ELBOW}  ${result}`)),
    onMaxTurns: () => console.log(dim("  stopped: hit the max turn limit.")),
    onTokenLimit: (message) => console.log(dim(`  stopped: hit the token limit. (${message})`)),
    confirmTool: async (name) => {
        const ans = (await rl.question(dim(`  run ${bold(name)}? [y/N] `))).trim().toLowerCase();
        return ans === "y" || ans === "yes";
    },
};

// ---- The chat loop ----
async function main() {
    printBanner();

    rl = readline.createInterface({
        input: process.stdin,
        output: process.stdout,
    });
    const messages: Anthropic.MessageParam[] = []; // working memory, kept across turns

    while (true) {
        const input = (await rl.question(bold("> "))).trim();
        if (!input) continue;

        const lower = input.toLowerCase();
        if (lower === "exit" || lower === "quit" || lower === "/exit") break;

        if (input.startsWith("/")) {
            handleCommand(input, messages);
            console.log();
            continue;
        }

        messages.push({ role: "user", content: input });
        await runAgent(messages, cli);
        console.log();
    }

    rl.close();
}

main();
