# 🐾 Monagotchi

A physical Tamagotchi on an ESP32 that's powered by a memecoin on [nad.fun](https://nad.fun) (Monad) and controls your real-world environment through Alexa. Managed via [OpenClaw](https://github.com/openclaw/openclaw).

**Every pet action burns $MONA tokens. Your pet's health controls your room.**

Feed it, play with it, heal it — each interaction burns $MONA on-chain. But neglect it and your room lights go red, the temperature climbs, and Alexa warns you. Take care of it and everything goes back to calm.

## How It Works

```
"Feed my pet"
    → OpenClaw checks pet status on ESP32
    → Burns 1,000 $MONA on Monad (sent to 0x...dEaD)
    → Tx confirms → calls ESP32 /feed endpoint
    → Pet reacts on LCD screen
    → Alexa sets room lights to green, thermostat to 72°F
    → Owner gets Telegram confirmation
```

```
Pet gets sick (neglected too long)
    → Alexa sets lights to red, thermostat to 80°F
    → "Warning. Monagotchi is sick. It needs healing."
```

```
Pet dies
    → Room goes full red, 85°F
    → Stays that way until you reset
```

## What's In This Repo

### ESP32 Firmware (`monagotchi.ino`)

Runs on an ESP32 + Waveshare 1.69" (240×280) TFT display + 5 physical buttons.

- **Pet simulation** — hunger, happiness, health, energy, cleanliness stats that decay over time
- **Animated pet** on LCD with state-based expressions (happy, sleeping, sick, hungry, dead)
- **5 hardware buttons** — feed, play, clean, sleep/wake, heal
- **Web server** on port 80 — HTTP API for remote control
- **Browser UI** — web dashboard with stat bars and action buttons
- **OpenClaw integration** — pushes status alerts to your AI agent

#### Hardware

| Component | Detail |
|-----------|--------|
| Display | Waveshare 1.69" TFT (240×280), SPI via TFT_eSPI |
| Button 1 — Feed | GPIO 32 |
| Button 2 — Play | GPIO 33 |
| Button 3 — Clean | GPIO 25 |
| Button 4 — Sleep | GPIO 26 |
| Button 5 — Heal | GPIO 27 |

### OpenClaw Skills

**`monagotchi-controller/`** — ESP32 HTTP API. Checks pet status, decides which action to take, enforces the rule: burn tokens first, then execute.

**`monagotchi-trader/`** — On-chain operations. Checks $MONA balance, burns tokens for pet actions via ERC-20 transfer to dead address. Uses ethers.js.

**`monagotchi-environment/`** — Alexa smart home bridge. Maps pet state to room lights, temperature, and voice announcements via [`alexacli`](https://clawhub.ai/buddyh/alexa-cli). Runs on every action and on a 5-minute cron.

### Token Economy

| Action | $MONA Burned | Pet Effect |
|--------|-------------|------------|
| Feed | 1,000 | +20 hunger |
| Play | 2,500 | +15 happiness, -15 energy |
| Clean | 1,500 | Cleanliness → 100, +5 health |
| Heal | 5,000 | Health → 100 |
| Sleep/Wake | free | Toggle rest (energy recovers) |
| Status | free | Check stats |

### Environment Mapping

| Pet State | Lights | Thermostat | Alexa Announcement |
|-----------|--------|-----------|-------------------|
| 🟢 Happy / Normal | Green | 72°F | "Monagotchi is happy and healthy." |
| 🟡 Hungry | Yellow | 76°F | "Monagotchi is getting hungry." |
| 🟠 Sleeping | Dim blue | 70°F | — (quiet) |
| 🔴 Sick | Red | 80°F | "Warning. Monagotchi is sick." |
| 💀 Dead | Full red | 85°F | "Monagotchi has died." (stays until reset) |

## Setup

### 1. Flash the ESP32

1. Open `monagotchi.ino` in Arduino IDE
2. Install ESP32 board support and [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
3. Configure TFT_eSPI for your Waveshare 1.69" display
4. Update WiFi credentials and OpenClaw webhook URL in the sketch
5. Flash and boot — the LCD shows the IP address

### 2. Launch $MONA on nad.fun

1. Go to [nad.fun](https://nad.fun), connect your Monad wallet
2. Create token → Name: **Monagotchi**, Symbol: **MONA**
3. Description: *"Every interaction burns $MONA. Care for the pet, burn the supply. Neglect it and your room pays the price."*
4. Do an initial buy so you have tokens to spend
5. Copy the token contract address

### 3. Install OpenClaw Skills

```bash
# Clone the repo
cd ~/.openclaw/skills
git clone https://github.com/YOUR_USERNAME/monagotchi.git

# Symlink skills so OpenClaw finds them
ln -s monagotchi/monagotchi-controller monagotchi-controller
ln -s monagotchi/monagotchi-trader monagotchi-trader
ln -s monagotchi/monagotchi-environment monagotchi-environment

# Install alexa-cli dependency
clawhub install buddyh/alexa-cli

# Install trader script dependencies
cd monagotchi/monagotchi-trader/scripts && npm install
```

### 4. Configure OpenClaw

Add to `~/.openclaw/openclaw.json`:

```json
{
  "skills": {
    "entries": {
      "monagotchi-controller": {
        "enabled": true,
        "env": {
          "MONAGOTCHI_ESP32_IP": "192.168.1.XXX"
        }
      },
      "monagotchi-trader": {
        "enabled": true,
        "env": {
          "MONAGOTCHI_TOKEN_ADDRESS": "0xYOUR_TOKEN_ADDRESS",
          "MONAD_PRIVATE_KEY": "0xYOUR_PRIVATE_KEY",
          "MONAD_RPC_URL": "https://monad-mainnet.g.alchemy.com/v2/YOUR_KEY"
        }
      },
      "monagotchi-environment": {
        "enabled": true,
        "env": {
          "MONAGOTCHI_ALEXA_DEVICE": "Bedroom"
        }
      },
      "alexa-cli": {
        "enabled": true
      }
    }
  }
}
```

Add the environment sync cron job:

```json
{
  "cron": {
    "jobs": [
      {
        "name": "monagotchi-environment-sync",
        "schedule": "*/5 * * * *",
        "command": "Check Monagotchi pet status and update room environment accordingly."
      }
    ]
  }
}
```

### 5. Test

Tell your OpenClaw agent: *"Feed my pet"*

It should:
1. Check pet status on the ESP32
2. Check your $MONA balance
3. Burn 1,000 $MONA (you'll see the tx hash)
4. Call the ESP32 `/feed` endpoint
5. Set your room lights to green via Alexa
6. Confirm via Telegram

Then wait. Don't feed it for a while. Watch your room slowly turn yellow, then red.

## Repo Structure

```
monagotchi/
├── monagotchi.ino                            # ESP32 firmware
├── monagotchi-controller/
│   └── SKILL.md                              # OpenClaw skill: ESP32 pet API
├── monagotchi-trader/
│   ├── SKILL.md                              # OpenClaw skill: $MONA token burns
│   ├── scripts/
│   │   ├── monagotchi-trader.mjs             # Burn + balance script
│   │   └── package.json
│   └── references/
│       └── nadfun-contracts.md               # Monad contract addresses
└── monagotchi-environment/
    └── SKILL.md                              # OpenClaw skill: Alexa environment
```

## Requirements

- ESP32 + Waveshare 1.69" TFT + 5 buttons
- [OpenClaw](https://github.com/openclaw/openclaw) running locally
- [alexa-cli](https://clawhub.ai/buddyh/alexa-cli) installed and authenticated
- Alexa-compatible smart lights + thermostat
- Monad wallet with $MONA + MON for gas
- Node.js 18+

## License

MIT
