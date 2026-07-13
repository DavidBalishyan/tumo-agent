# TUMO agent

A small, hackable AI agent written in TypeScript. It runs a chat loop in your
terminal, calls tools, and remembers things between runs. Think of it as your
own tiny Claude Code.

Built from the Day 5 template as the Week 2 starting point. [tumo-ai-agents](https://github.com/rhovagimian/tumo-ai-agents)

## Setup
```bash
npm install
cp .env.example .env   # then paste your key into .env
# (or export it directly: export ANTHROPIC_API_KEY="your-key-here")
```

## Run
```bash
npm start
```

This opens a chat loop. Type at the `>` prompt. `⏺` marks something the agent
did (a thought, or a tool call), and `⎿` shows what came back.

```
> My name is Ani, what's the weather in Yerevan?

⏺ I'll save your name, then check the weather.

⏺ remember({"fact":"The user is named Ani."})
  ⎿  Saved to memory: The user is named Ani.

⏺ get_weather({"city":"Yerevan"})
  ⎿  28°C, sunny

⏺ Nice to meet you, Ani! It's 28°C and sunny in Yerevan.
```

The conversation stays in memory across turns. The `remember` tool writes facts
to `memory.json` so they survive a restart.

## The agent recipe
An agent needs four things:
1. Tools are the functions Claude can request (`src/tools.ts`).
2. The loop calls the model, runs whatever tools it asks for, then repeats (`src/agent.ts`).
3. The prompt sets the plan, personality, and rules (`SYSTEM_PROMPT` in `src/agent.ts`).
4. Memory is the `messages` array for this chat, plus `memory.json` for facts that outlast it.

## How it works
```
you type ──▶ messages[] ──▶ Claude
                              │
              ┌───────────────┴───────────────┐
        wants a tool?                     just talking?
              │                                │
        run the tool                     print the answer
        push the result ──▶ back to Claude     │
              └────────── loop ──────────▶ done ┘
```
The loop runs until Claude stops asking for tools, or until it hits the 10-turn
safety limit.

## Files
| File | What it does |
|------|-------------|
| `src/core.ts` | The headless agent loop. Reports events through handlers; no terminal code |
| `src/agent.ts` | The CLI front-end: maps core events to terminal output, plus the chat loop |
| `src/serve.ts` | A JSON-over-stdio adapter so other programs can drive the agent |
| `src/tools.ts` | Tool definitions, their functions, and the dispatcher |
| `src/commands.ts` | Slash commands, colors, and the startup banner |
| `gui/` | A native C + raylib desktop front-end (see below) |

The agent core (`src/core.ts`) knows nothing about how it's displayed. It reports
what it's doing through a small set of handlers, and a front-end decides what to
do with them:

```ts
runAgent(messages, {
    onText:       (delta)  => /* a streamed chunk of the reply */,
    onToolCall:   (name, input)  => /* Claude wants to run a tool */,
    onToolResult: (name, result) => /* what the tool returned */,
    // ... onTextStart, onTextEnd, onMaxTurns
});
```

`agent.ts` maps those to `stdout`. `serve.ts` maps them to JSON lines. Any other
UI is just a new adapter.

## Commands
| Command | What it does |
|---------|-------------|
| `/help` | List the commands |
| `/model [name]` | Switch model, or show the current one. Try `opus`, `sonnet`, `haiku`, or any full `claude-*` id |
| `/models` | List the models you can switch to (`*` marks the current one) |
| `/memory` | Show what's in long-term memory |
| `/cwd` | Show the working directory |
| `/cd <path>` | Change the working directory (re-roots what the file tools can reach) |
| `/save <name>` | Save this conversation to `sessions/<name>.json` |
| `/load <name>` | Restore a saved conversation (the agent picks it back up with full context) |
| `/sessions` | List saved conversations |
| `/clear` | Forget this conversation (long-term memory survives) |
| `/exit` | Quit (so do `exit` and `quit`) |

## Built-in tools
| Tool | What it does |
|------|-------------|
| `calculator` | Exact arithmetic (add, subtract, multiply, divide) |
| `get_weather` | Weather for a city (demo data) |
| `dictionary` | Definition of an English word (demo data) |
| `remember` | Saves a fact to `memory.json` so it survives a restart |
| `read_file` | Read a text file, relative to the project directory |
| `list_dir` | List a directory, relative to the project directory |
| `write_file` | Write a text file (asks you to confirm first) |
| `run_command` | Run a shell command in the project directory (asks you to confirm first) |

The file tools are scoped to the project directory: a path that resolves
outside it is rejected. Tools that change something on disk are marked dangerous
(see `DANGEROUS_TOOLS` in `src/tools.ts`), and the agent asks before running
one. In the CLI you get a `y/N` prompt; in the GUI the input bar turns into a
`[y] yes  [n] no` prompt.

## Make it your own
1. Edit `SYSTEM_PROMPT` in `src/agent.ts` to give your agent a job and personality.
2. Add a tool in `src/tools.ts` (a definition, a function, and a dispatcher case).
   Add its name to `DANGEROUS_TOOLS` if it changes something.
3. Add a slash command in `src/commands.ts`.
4. Run `npm start` and chat with it.

From here you can add more real tools (a shell command, a web fetch) and give
the agent smarter long-term memory. Streaming, file tools, and tool
confirmation are already wired up.

## Native GUI (C + raylib)
`gui/` is a desktop window written in C with [raylib](https://www.raylib.com/).
It's a thin front end over the same agent: it launches the Node bridge
(`src/serve.ts`) as a child process, sends your messages as JSON, and reads the
event stream back to draw the chat. Replies stream the same way they do in the
terminal.

```bash
cd gui
./fetch-deps.sh   # downloads raylib, cJSON, and JetBrainsMono Nerd Font into
                  # gui/vendor/ (needs curl and unzip)
make              # builds ./tumo-gui
make run          # or ./tumo-gui
```

Sources live in `gui/src/`, object files build into `gui/build/`, and the only
product left at the `gui/` root is the `tumo-gui` binary. The C code is split
into small modules, each with its own header:

| Unit | What it does |
|------|-------------|
| `src/paths.c` | Finds the project root from the executable's own path |
| `src/chat.c` | The message transcript (the data model) |
| `src/bridge.c` | Launches the Node agent and turns its JSON events into messages |
| `src/ui.c` | Font loading and drawing the chat with raylib |
| `src/main.c` | The window and the main loop, wiring the others together |

You need a C compiler and the X11/OpenGL dev libraries (`libgl1-mesa-dev`,
`libx11-dev` on Debian/Ubuntu). The GUI finds the project root from its own
path, so it reads the same `.env` and `memory.json` as the CLI, and you can
launch it from anywhere. Text is drawn in JetBrainsMono Nerd Font; if the ttf is
missing it falls back to raylib's built-in font.

The same slash commands as the CLI work here. Type `/help`, `/model opus`,
`/models`, `/memory`, or `/clear` and the window forwards them to the agent.
`/exit` (or `exit`) closes the window.

Zoom the text with `Ctrl` and `+` / `-`, or `Ctrl` + the scroll wheel. The font
is re-rasterized at the new size each step, so it stays sharp rather than
scaling a fixed image. The scroll wheel alone scrolls the transcript.

The agent's replies are rendered as Markdown: `#` headings (sized), `**bold**`,
`*italic*`, `` `inline code` `` with a background, `[links](url)`, bullet and
numbered lists with hanging indents, `>` blockquotes, `---` rules, and fenced
code blocks. Bold and italic use real font weights (JetBrainsMono Nerd Font
Regular/Bold/Italic/BoldItalic). Everything wraps to the window, and long
unbroken tokens (like tool-call JSON) break character by character so nothing
runs off the edge.

Unicode renders too. Glyphs are loaded on demand: the app scans the chat for the
codepoints that actually appear and builds the font atlas to match, so accents,
Greek, Cyrillic, arrows, and math symbols all show. Emoji fall back to Noto Emoji
and render in a single colour (raylib can't do colour emoji). CJK has no bundled
font, so those characters won't show.

How the two sides talk (newline-delimited JSON):

```
C GUI  ──▶  {"type":"user","text":"hi"}          ──▶  node src/serve.ts
       ◀──  {"type":"text","delta":"He"}          ◀──
       ◀──  {"type":"text","delta":"llo"}         ◀──
       ◀──  {"type":"tool_call","name":"...",...} ◀──
       ◀──  {"type":"turn_end"}                   ◀──   (ready for the next message)
```

The same `serve.ts` bridge works for any language, so a Rust, Swift, or C# front
end would talk to it exactly the same way.
