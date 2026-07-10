import "dotenv/config";
import Anthropic from "@anthropic-ai/sdk";
import * as readline from "readline/promises";
import { allTools, runTool, loadMemory } from "./tools";
import { dim, bold, DOT, ELBOW, getModel, handleCommand, printBanner } from "./commands";

// ============================================================
// THE COMPLETE AGENT — a chat loop, like Claude Code.
//
//   tools (from tools.ts)      -> abilities
//   commands (commands.ts)     -> /model, /memory, /clear, ...
//   messages[]                 -> working memory (this conversation)
//   memory.json                -> long-term memory (across runs)
//   the agent loop             -> think, act, observe, repeat
//
// TO MAKE IT YOUR OWN:
//   1. Edit SYSTEM_PROMPT to give your agent a job + personality.
//   2. Add tools in tools.ts (function + definition + dispatcher case).
//   3. Run: npm run agent
// ============================================================

const client = new Anthropic();

// ---- Customize your agent here! ----
const SYSTEM_PROMPT = `You are a helpful assistant agent with access to tools.

How to work:
1. Briefly plan your approach before acting.
2. Use tools one step at a time when they help.
3. When the user shares something worth keeping (their name, preferences,
   goals), call the remember tool.
4. When the task is done, give a clear, friendly answer.`;

// Long-term memory is folded into the prompt on every call.
function buildSystemPrompt(): string {
  const facts = loadMemory();
  if (facts.length === 0) return SYSTEM_PROMPT;
  return SYSTEM_PROMPT + "\n\nThings you remember: " + facts.join(" ");
}

// ---- The agent loop (with guardrails) ----
// STRETCH: stream Claude's text so it types out live. Swap
// client.messages.create(...) for client.messages.stream(...), pipe
// stream.on("text", ...) to stdout, and read the finished turn from
// await stream.finalMessage().
async function runAgent(
  messages: Anthropic.MessageParam[],
  maxTurns = 10
): Promise<string> {
  let turns = 0;
  while (turns < maxTurns) {
    turns++;
    const res = await client.messages.create({
      model: getModel(),
      max_tokens: 1024,
      system: buildSystemPrompt(),
      tools: allTools,
      messages,
    });
    messages.push({ role: "assistant", content: res.content });

    if (res.stop_reason !== "tool_use") {
      const text = res.content.find((c) => c.type === "text");
      return text && text.type === "text" ? text.text : "";
    }

    // Claude's plan for this step
    for (const block of res.content) {
      if (block.type === "text") console.log(`\n${DOT} ${block.text}`);
    }

    // Run every tool Claude asked for, show the call, send results back
    const toolResults: Anthropic.ToolResultBlockParam[] = [];
    for (const block of res.content) {
      if (block.type !== "tool_use") continue;
      let result: string;
      try {
        result = runTool(block.name, block.input);
      } catch (e: any) {
        result = "Tool error: " + e.message;
      }
      const args = JSON.stringify(block.input);
      console.log(`\n${DOT} ${bold(block.name)}(${dim(args)})`);
      console.log(dim(`  ${ELBOW}  ${result}`));
      toolResults.push({
        type: "tool_result",
        tool_use_id: block.id,
        content: result,
      });
    }
    messages.push({ role: "user", content: toolResults });
  }
  return "Stopped: hit the max turn limit.";
}

// ---- The chat loop ----
async function main() {
  printBanner();

  const rl = readline.createInterface({
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
    const answer = await runAgent(messages);
    console.log(`\n${DOT} ${answer}\n`);
  }

  rl.close();
}

main();
