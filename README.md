# Full Agent

Your Week 2 starting point — the complete agent template from Day 5, ready to
fork and make your own.

## Setup
```bash
npm install
cp .env.example .env   # then paste your key into .env
# (or export it directly: export ANTHROPIC_API_KEY="your-key-here")
```

## Run
```bash
npm run agent
```

This opens a chat loop — your own little Claude Code. Type at the `>` prompt.
`⏺` marks something the agent did (a thought, or a tool call), and `⎿` shows
what came back.

```
> My name is Ani, what's the weather in Yerevan?

⏺ I'll save your name, then check the weather.

⏺ remember({"fact":"The user is named Ani."})
  ⎿  Saved to memory: The user is named Ani.

⏺ get_weather({"city":"Yerevan"})
  ⎿  28°C, sunny

⏺ Nice to meet you, Ani! It's 28°C and sunny in Yerevan.
```

The conversation stays in memory across turns; the `remember` tool writes facts
to `memory.json` so they survive a restart.

## Files
| File | What it does |
|------|-------------|
| `src/agent.ts` | The chat loop + the agent loop (think, act, observe, repeat) |
| `src/tools.ts` | Shared tools + the dispatcher |
| `src/commands.ts` | Slash commands for the chat loop |

## The Agent Recipe
Every agent = four ingredients:
1. **Tools** — functions Claude can request
2. **Loop** — call, run tools, repeat
3. **Prompt** — plan + personality + rules
4. **Memory** — remember what matters

## Commands
| Command | What it does |
|---------|-------------|
| `/help` | List the commands |
| `/model [name]` | Switch model, or show the current one. Try `opus`, `sonnet`, `haiku`, or any full `claude-…` id |
| `/models` | List the models you can switch to (`*` marks the current one) |
| `/memory` | Show what's in long-term memory |
| `/clear` | Forget this conversation (long-term memory survives) |
| `/exit` | Quit — so do `exit` and `quit` |

## Make It Your Own
1. Edit `SYSTEM_PROMPT` in `agent.ts` to give your agent a job + personality.
2. Add your own tools in `tools.ts` (function + definition + dispatcher case).
3. Add your own slash command in `commands.ts`.
4. Run `npm run agent` and chat with your creation!
