# monagotchi-ausd

Watches for AUSD transfers to the Monagotchi treasury and logs wallet attribution for the Moltiverse/Agora integration.

## What this skill does

- Monitors AUSD (Agora USD) transfers to the treasury address on Monad
- Logs each sender's wallet, amount, and tx hash to `ausd_log.json`
- Updates pet state with a `AUSD_FEED` event

## Skill: add_ausd

**Triggered when:** AUSD is sent to the Monagotchi treasury address.

**What it does:**
1. Records the sender wallet, amount, and tx hash
2. Appends the entry to `scripts/ausd_log.json`
3. Reports success back to the agent

**Usage:**
> "Start the AUSD watcher"
> "Who has sent AUSD to Monagotchi?"
> "Show AUSD contributors"

## Setup

### 1. Install dependencies

```bash
cd monagotchi-ausd/scripts && npm install
```

### 2. Configure OpenClaw

Add to `~/.openclaw/openclaw.json`:

```json
{
  "skills": {
    "entries": {
      "monagotchi-ausd": {
        "enabled": true,
        "env": {
          "TREASURY_ADDRESS": "0xYOUR_TREASURY_WALLET",
          "MONAD_RPC_URL": "https://monad-mainnet.g.alchemy.com/v2/YOUR_KEY"
        }
      }
    }
  }
}
```

### 3. Run the watcher

```bash
node monagotchi-ausd/scripts/ausd-watcher.mjs
```

Or let OpenClaw manage it via the skill.

## Output

Logs are written to `scripts/ausd_log.json`:

```json
[
  {
    "wallet": "0xabc...",
    "amount": "10000000",
    "amountFormatted": "10.00",
    "txHash": "0xdef...",
    "timestamp": "2025-01-01T00:00:00.000Z"
  }
]
```

## AUSD Contract

- **Address:** `0x00000000eFE302BEAA2b3e6e1b18d08D69a9012a`
- **Network:** Monad Mainnet
- **Decimals:** 6
