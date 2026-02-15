---
name: monagotchi-trader
description: Manage $MONA tokens on nad.fun (Monad blockchain) for Monagotchi pet interactions. Use when you need to burn tokens for pet actions, check token balance, or get token info. Every pet action (feed, play, clean, heal) requires burning $MONA tokens through this skill. Works with monagotchi-controller.
metadata:
  openclaw:
    emoji: "🪙"
    requires:
      bins: ["node"]
    env:
      - MONAGOTCHI_TOKEN_ADDRESS
      - MONAD_PRIVATE_KEY
      - MONAD_RPC_URL
---

# Nad.fun Monagotchi Trader

Manage $MONA tokens on the Monad blockchain for Monagotchi pet interactions. This skill handles burning tokens when the owner wants to feed, play with, clean, or heal their pet.

## Overview

$MONA is the fuel for your Monagotchi. Every pet action burns tokens — they're sent to the dead address and permanently removed from supply. The more you care for your pet, the scarcer $MONA becomes.

## Setup

Install dependencies first:

```bash
cd {baseDir}/scripts && npm install
```

## Configuration

Required environment variables (set in `~/.openclaw/openclaw.json` under `skills.entries.monagotchi-trader.env`):

```
MONAGOTCHI_TOKEN_ADDRESS=0x...       # $MONA token contract on Monad
MONAD_PRIVATE_KEY=0x...              # Owner wallet private key (for signing burn txs)
MONAD_RPC_URL=https://...            # Monad RPC endpoint
```

## Action Costs

| Action | $MONA Burned |
|--------|-------------|
| Feed | 1,000 |
| Play | 2,500 |
| Clean | 1,500 |
| Heal | 5,000 |
| Sleep/Wake | 0 (free) |
| Status | 0 (free) |

These costs are defined in the `{baseDir}/scripts/monagotchi-trader.mjs` config and can be adjusted.

## How Burning Works

Burning = transferring tokens to the dead address:
```
0x000000000000000000000000000000000000dEaD
```

This is a standard ERC-20 `transfer()` call. Tokens sent to the dead address are permanently out of circulation.

## Operations

### Check Balance

```bash
node {baseDir}/scripts/monagotchi-trader.mjs balance
```

Returns JSON with the owner's $MONA balance and what they can afford.

### Burn Tokens (for a pet action)

```bash
node {baseDir}/scripts/monagotchi-trader.mjs burn feed
node {baseDir}/scripts/monagotchi-trader.mjs burn play
node {baseDir}/scripts/monagotchi-trader.mjs burn clean
node {baseDir}/scripts/monagotchi-trader.mjs burn heal
```

Each command:
1. Checks the owner's $MONA balance
2. Verifies sufficient tokens for the action
3. Sends the required amount to 0x...dEaD
4. Waits for tx confirmation
5. Outputs JSON with the tx hash on success or an error message on failure

### Get Token Info

```bash
node {baseDir}/scripts/monagotchi-trader.mjs info
```

Returns token name, symbol, total supply, owner balance, and bonding curve progress.

## Output Format

All commands output JSON to stdout. Status/debug messages go to stderr. Parse stdout for structured results:

```json
{"success": true, "action": "feed", "burned": 1000, "txHash": "0x...", "remainingBalance": "45000.0"}
```

On failure:
```json
{"success": false, "error": "Insufficient $MONA. Need 1000, have 500.0"}
```

## Contract Details

### Monad Mainnet Addresses

See `{baseDir}/references/nadfun-contracts.md` for full list. Key addresses:

```
BONDING_CURVE:  0xA7283d07812a02AFB7C09B60f8896bCEA3F90aCE
LENS:           0x7e78A8DE94f21804F7a17F4E8BF9EC2c872187ea
BURN_ADDRESS:   0x000000000000000000000000000000000000dEaD
```

## Security

⚠️ **The private key is stored as an environment variable.** This is the owner's wallet key used to sign burn transactions. Keep it safe:

- Never commit it to git
- Set it in `~/.openclaw/openclaw.json` under `skills.entries.monagotchi-trader.env`
- Consider using a dedicated wallet with only $MONA and a small amount of MON for gas

## Integration with monagotchi-controller

The typical flow when the owner requests a pet action:

1. **monagotchi-controller** checks pet status via `curl -s http://${MONAGOTCHI_ESP32_IP}/pet`
2. **monagotchi-controller** determines which action to take and its cost
3. **monagotchi-trader** checks $MONA balance — `node {baseDir}/scripts/monagotchi-trader.mjs balance`
4. **monagotchi-trader** burns the required $MONA — `node {baseDir}/scripts/monagotchi-trader.mjs burn <action>`
5. If burn tx confirms → **monagotchi-controller** calls the ESP32 endpoint
6. If burn fails → report the error, don't execute the action
7. **monagotchi-environment** updates room lights/temp based on pet state

## Examples

- "Feed my pet" → `node {baseDir}/scripts/monagotchi-trader.mjs burn feed` → on success, controller calls POST /feed
- "What's my $MONA balance?" → `node {baseDir}/scripts/monagotchi-trader.mjs balance`, report back
- "Heal and clean my pet" → burn 5,000 for heal → call /heal → burn 1,500 for clean → call /clean
- "Can I afford to play?" → check balance → compare against 2,500 → respond yes/no
