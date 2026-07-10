# Full Agent

A small, hackable AI agent in about 250 lines of TypeScript. It runs a chat loop
in your terminal, calls tools, and remembers things between runs. Think of it as
your own tiny Claude Code.

Built from the Day 5 template as the Week 2 starting point. See [`PLAN.md`](./PLAN.md)
for what to build next.

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
| `src/agent.ts` | The chat loop plus the agent loop (think, act, observe, repeat) |
| `src/tools.ts` | Tool definitions, their functions, and the dispatcher |
| `src/commands.ts` | Slash commands, colors, and the startup banner |

## Commands
| Command | What it does |
|---------|-------------|
| `/help` | List the commands |
| `/model [name]` | Switch model, or show the current one. Try `opus`, `sonnet`, `haiku`, or any full `claude-*` id |
| `/models` | List the models you can switch to (`*` marks the current one) |
| `/memory` | Show what's in long-term memory |
| `/clear` | Forget this conversation (long-term memory survives) |
| `/exit` | Quit (so do `exit` and `quit`) |

## Built-in tools
| Tool | What it does |
|------|-------------|
| `calculator` | Exact arithmetic (add, subtract, multiply, divide) |
| `get_weather` | Weather for a city (demo data) |
| `dictionary` | Definition of an English word (demo data) |
| `remember` | Saves a fact to `memory.json` so it survives a restart |

## Make it your own
1. Edit `SYSTEM_PROMPT` in `src/agent.ts` to give your agent a job and personality.
2. Add a tool in `src/tools.ts` (a definition, a function, and a dispatcher case).
3. Add a slash command in `src/commands.ts`.
4. Run `npm start` and chat with it.

Then work through [`PLAN.md`](./PLAN.md): real tools, safer tool permissions, and
smarter memory. (Streaming is already wired up.)
