import "dotenv/config";
import Anthropic from "@anthropic-ai/sdk";
import * as readline from "readline";
import * as fs from "fs";
import { Resvg } from "@resvg/resvg-js";
import { runAgent, AgentHandlers } from "./core";
import { runCommand } from "./commands";

// ============================================================
// LaTeX math rendering via MathJax + resvg.
// MathJax converts LaTeX to SVG, resvg rasterizes SVG to PNG.
// ============================================================

let mjReady: Promise<any> | null = null;

async function getMathJax(): Promise<any> {
  if (!mjReady) {
    mjReady = (async () => {
      const { init } = await import("mathjax-full/es5/node-main.js");
      const MathJax = await init({
        loader: { load: ["input/tex", "output/svg"] },
        tex: { packages: { "[+]": ["ams", "noerrors", "noundefined"] } },
        svg: { fontCache: "none" },
      });
      return MathJax;
    })();
  }
  return mjReady;
}

async function renderMath(latex: string, display: boolean, color?: string): Promise<{ width: number; height: number; data: string }> {
  const MathJax = await getMathJax();
  const svg = MathJax.tex2svg(latex, { display });
  const svgHtml = MathJax.startup.adaptor.innerHTML(svg);

  // Inject text color into the SVG so glyphs aren't always black.
  // MathJax output uses "currentColor" and already has a style attribute.
  // Append our color into the existing style.
  const colored = color
    ? svgHtml.replace('style="', `style="color:${color};`)
    : svgHtml;

  // SVG natural width (in ex units) tells us the pixel width at 1ex=~font size.
  // Render at a high DPI so long equations stay crisp; the C side scales down.
  const resvg = new Resvg(colored, {
    fitTo: { mode: "width", value: 1200 },
    background: "rgba(0,0,0,0)",
  });
  const rendered = resvg.render();
  const png = rendered.asPng();
  const b64 = png.toString("base64");
  return { width: rendered.width, height: rendered.height, data: b64 };
}

// ============================================================
// The stdio protocol adapter. Same idea as the CLI adapter in
// agent.ts, but instead of printing to a terminal it speaks
// newline-delimited JSON: one JSON object per line, both ways.
//
// A native GUI (C + raylib, Rust, Swift, ...) launches this as a
// child process, writes user messages to its stdin, and reads the
// event stream from its stdout.
//
//   stdout is the protocol channel, so NOTHING else may be written
//   there. Diagnostics go to stderr via console.error.
//
// Events out (node -> GUI):
//   {"type":"ready"}
//   {"type":"thinking_start"}
//   {"type":"thinking","delta":"..."}
//   {"type":"thinking_end"}
//   {"type":"text_start"}
//   {"type":"text","delta":"..."}
//   {"type":"text_end"}
//   {"type":"tool_call","name":"...","input":{...}}
//   {"type":"tool_result","name":"...","result":"..."}
//   {"type":"max_turns"}
//   {"type":"token_limit","message":"..."}
//   {"type":"error","message":"..."}
//   {"type":"turn_end"}          // safe to send the next message
//   {"type":"command_result","lines":[...],"cleared":bool}
//   {"type":"confirm","name":"...","input":{...}}  // asking to run a tool
//   {"type":"math_rendered","id":...,"width":...,"height":...,"data":"<base64>"}
//
// Commands in (GUI -> node):
//   {"type":"user","text":"..."}
//   {"type":"command","text":"/model opus"}
//   {"type":"confirm_response","approved":bool}
//   {"type":"render_math","id":...,"latex":"...","display":bool}
//   {"type":"exit"}
// ============================================================

function emit(obj: object): void {
    process.stdout.write(JSON.stringify(obj) + "\n");
}

// A dangerous tool is waiting on the front-end's yes/no. confirmTool parks a
// resolver here and returns a promise; the confirm_response line settles it.
let pendingConfirm: ((approved: boolean) => void) | null = null;

const protocol: AgentHandlers = {
    onThinkingStart: () => emit({ type: "thinking_start" }),
    onThinking: (delta) => emit({ type: "thinking", delta }),
    onThinkingEnd: () => emit({ type: "thinking_end" }),
    onTextStart: () => emit({ type: "text_start" }),
    onText: (delta) => emit({ type: "text", delta }),
    onTextEnd: () => emit({ type: "text_end" }),
    onToolCall: (name, input) => emit({ type: "tool_call", name, input }),
    onToolResult: (name, result) => emit({ type: "tool_result", name, result }),
    onMaxTurns: () => emit({ type: "max_turns" }),
    onTokenLimit: (message) => emit({ type: "token_limit", message }),
    confirmTool: (name, input) =>
        new Promise<boolean>((resolve) => {
            pendingConfirm = resolve;
            emit({ type: "confirm", name, input });
        }),
};

const messages: Anthropic.MessageParam[] = []; // conversation, kept across turns

// One turn at a time. Messages that arrive mid-turn wait in the queue.
let busy = false;
const queue: string[] = [];

async function pump(): Promise<void> {
    if (busy) return;
    busy = true;
    while (queue.length > 0) {
        const text = queue.shift()!;
        messages.push({ role: "user", content: text });
        try {
            await runAgent(messages, protocol);
        } catch (e: any) {
            emit({ type: "error", message: e?.message ?? String(e) });
        }
        emit({ type: "turn_end" });
    }
    busy = false;
}

const rl = readline.createInterface({ input: process.stdin });

rl.on("line", (line) => {
    const trimmed = line.trim();
    if (!trimmed) return;

    let msg: any;
    try {
        msg = JSON.parse(trimmed);
    } catch {
        emit({ type: "error", message: "invalid JSON on stdin" });
        return;
    }

    if (msg.type === "user" && typeof msg.text === "string") {
        queue.push(msg.text);
        pump();
    } else if (msg.type === "command" && typeof msg.text === "string") {
        // Slash commands are cheap and synchronous. Refuse mid-turn so /clear
        // can't yank the messages array out from under a running turn.
        if (busy) {
            emit({ type: "command_result", lines: ["busy, try again after the reply"], cleared: false });
        } else {
            const { lines, cleared } = runCommand(msg.text, messages);
            emit({ type: "command_result", lines, cleared });
        }
    } else if (msg.type === "confirm_response") {
        // settle a pending tool-approval prompt
        if (pendingConfirm) {
            const resolve = pendingConfirm;
            pendingConfirm = null;
            resolve(!!msg.approved);
        }
    } else if (msg.type === "render_math" && typeof msg.latex === "string") {
        // Render LaTeX to PNG in the background; the result arrives whenever it's ready.
        const color = typeof msg.color === "string" ? msg.color : undefined;
        renderMath(msg.latex, !!msg.display, color)
            .then((result) => emit({ type: "math_rendered", id: msg.id, ...result }))
            .catch((e: any) =>
                emit({ type: "error", message: `math render failed: ${e?.message ?? e}` })
            );
    } else if (msg.type === "exit") {
        process.exit(0);
    } else {
        emit({ type: "error", message: "unknown command" });
    }
});

// If our stdin closes, the front-end is gone (it may have been killed before it
// could send {"type":"exit"}). Don't linger as an orphan.
rl.on("close", () => process.exit(0));

emit({ type: "ready" });
