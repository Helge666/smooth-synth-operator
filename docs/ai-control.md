# AI control (MCP)

Each SSO app can let an AI assistant such as Claude program the synth: read and change every
sound parameter, play notes so you can hear the result, use the sound library and the
generator. It uses the Model Context Protocol (MCP), the standard way for AI programs to use
tools.

What the AI can do:

| Tool | |
|---|---|
| `synth_info` | the synth in a compact table: voices, signal flow, quirks, every parameter with range and function |
| `get_sound`, `set_parameters`, `set_sound_name`, `init_sound` | read and change the current sound |
| `play_notes`, `stop_sound` | play notes or a short phrase (at most 30 s) |
| `list_presets`, `load_preset`, `save_preset` | the app's library (saving goes to bank "AI Sounds" by default) |
| `generate_sound`, `vary_sound` | the app's generator |

What it cannot do: write to the synth's memory or its global settings, touch files outside the
library, or be reached from another computer. Changes go to the synth's edit buffer, exactly
like turning the app's knobs, and show in the app as they happen.

## 1. Switch it on

Open the app (standalone, or the plugin in your DAW), click **MIDI** and tick
**Let AI assistants on this computer control the synth**. The line below shows the address and,
later, which AI program used it. An **AI** light in the header flickers when the AI does
something. The setting is remembered; it is off until you switch it on.

| App | Address |
|---|---|
| SSO Kiwi-3P | `http://127.0.0.1:47831/mcp` |
| SSO KiwiSix | `http://127.0.0.1:47832/mcp` |
| SSO Hawk-800 | `http://127.0.0.1:47833/mcp` |

If several instances of one app run (e.g. two Cubase tracks), the one that controls the synth
answers.

## 2. Connect your AI program

**Claude Code** (and other programs that accept an MCP server URL, "Streamable HTTP"):

```
claude mcp add --transport http --scope user sso-hawk800 http://127.0.0.1:47833/mcp
```

**Claude Desktop** (and other programs that start MCP servers as programs, "stdio"): they need
the small bridge `sso-mcp.exe`, which `install-vst3.ps1` copies to
`C:\Program Files\Smooth Synth Operator\`. In Claude Desktop open *Settings > Developer >
Edit Config* and add one entry per synth:

```json
{
  "mcpServers": {
    "sso-hawk800": {
      "command": "C:\\Program Files\\Smooth Synth Operator\\sso-mcp.exe",
      "args": ["hawk800"]
    },
    "sso-kiwi3p": {
      "command": "C:\\Program Files\\Smooth Synth Operator\\sso-mcp.exe",
      "args": ["kiwi3p"]
    },
    "sso-kiwisix": {
      "command": "C:\\Program Files\\Smooth Synth Operator\\sso-mcp.exe",
      "args": ["kiwisix"]
    }
  }
}
```

Restart Claude Desktop afterwards.

## 3. Use it

Start the app first, then ask, for example:

- "Look at my Poly-800 and make a slow string pad with a gentle filter sweep."
- "Play a C major chord so I can hear it." – the AI cannot hear the synth; tell it what you hear.
- "Make the attack softer and save it as 'Evening Strings'."
- "Generate three bass sounds one after another and play each."

## Troubleshooting

- **"... is not reachable"**: the app is not running, AI control is off, or another instance
  controls the synth (TAKE OVER).
- **"Port ... is used by another program"** in the MIDI window: another program already uses
  that port; the app tries again every 5 seconds.
- **The AI says it cannot change anything while tuning**: TUNE is on; switch it off.
- **Nothing sounds**: check the MIDI ports and channel in the MIDI window first; the AI uses the
  same connection as the app.
