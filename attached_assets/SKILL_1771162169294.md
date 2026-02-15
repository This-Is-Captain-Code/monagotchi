---
name: monagotchi-controller
description: Control the Monagotchi virtual pet running on an ESP32 with LCD display. Use when the owner wants to feed, play with, clean, put to sleep, heal, or check status of their pet. Every action except sleep and status costs $MONA tokens which get burned on-chain. Works with monagotchi-trader and monagotchi-environment skills.
metadata: {"openclaw": {"emoji": "🐾", "requires": {"env": ["MONAGOTCHI_ESP32_IP"]}}}
---

# Monagotchi Controller

Control a physical Monagotchi virtual pet running on an ESP32 microcontroller with a 1.69" LCD display and 5 hardware buttons.

## What This Does

This skill lets you interact with the Monagotchi ESP32 device over your local network. The ESP32 runs a web server that accepts HTTP requests to feed, play with, clean, heal, and put the pet to sleep.

**Every action costs $MONA tokens.** Before calling the ESP32, you must burn the required amount of $MONA using the `monagotchi-trader` skill. If the burn succeeds, execute the action. If it fails, don't.

## Configuration

The ESP32's local IP address must be set:

```
MONAGOTCHI_ESP32_IP=192.168.1.XXX
```

## Action Costs

| Action | $MONA Cost | Endpoint | Effect |
|--------|-----------|----------|--------|
| Feed | 1,000 | POST /feed | +20 hunger, -5 energy |
| Play | 2,500 | POST /play | +15 happiness, -15 energy, -10 hunger |
| Clean | 1,500 | POST /clean | Cleanliness → 100, +5 health |
| Heal | 5,000 | POST /heal | Health → 100, state → normal |
| Sleep/Wake | free | POST /sleep | Toggle sleep (energy recovers while sleeping) |
| Status | free | GET /pet | Check all stats |
| Reset | free | POST /reset | New pet, fresh stats |

## Flow for Every Paid Action

1. Owner says "feed my pet" (or any action)
2. Check pet status first — `curl -s http://${MONAGOTCHI_ESP32_IP}/pet`
3. If action makes sense (pet is alive, stats warrant it), use `monagotchi-trader` skill to burn the required $MONA
4. If burn tx succeeds → call the ESP32 endpoint
5. If burn fails (insufficient balance, tx error) → tell the owner, don't call the ESP32
6. Report result back

**Always burn BEFORE calling the ESP32. No free rides.**

## Pet Stats (0–100)

- **Hunger** — Decreases by 5/min awake, 2/min sleeping. Feed to restore.
- **Happiness** — Decreases by 3/min awake, 1/min sleeping. Play to restore.
- **Health** — Drops by 5/min if hunger or cleanliness falls below 20. Heal to restore.
- **Energy** — Decreases by 2/min awake. Recovers +10/min sleeping. Pet auto-sleeps below 20.
- **Cleanliness** — Decreases by 4/min awake, 2/min sleeping. Clean to restore.

If health reaches 0, the pet dies. Reset to start over.

## Pet States

- `normal` — Default state
- `eating` — Just fed (temporary)
- `playing` — Just played with (temporary)
- `sleeping` — Resting, energy recovering
- `hungry` — Hunger below 20
- `sick` — Health below 30
- `dead` — Health reached 0. Only reset works.

## Endpoints

### Check Status (free)

```bash
curl -s http://${MONAGOTCHI_ESP32_IP}/pet | jq .
```

Returns:
```json
{
  "hunger": 50,
  "happiness": 50,
  "health": 80,
  "energy": 100,
  "cleanliness": 80,
  "age": 12,
  "isAlive": true,
  "state": "normal"
}
```

### Feed — 1,000 $MONA

```bash
curl -s -X POST http://${MONAGOTCHI_ESP32_IP}/feed
```

### Play — 2,500 $MONA (requires energy ≥ 10)

```bash
curl -s -X POST http://${MONAGOTCHI_ESP32_IP}/play
```

### Clean — 1,500 $MONA

```bash
curl -s -X POST http://${MONAGOTCHI_ESP32_IP}/clean
```

### Heal — 5,000 $MONA

```bash
curl -s -X POST http://${MONAGOTCHI_ESP32_IP}/heal
```

### Sleep / Wake (free)

```bash
curl -s -X POST http://${MONAGOTCHI_ESP32_IP}/sleep
```

### Reset (free)

```bash
curl -s -X POST http://${MONAGOTCHI_ESP32_IP}/reset
```

## Response Format

All action endpoints return:
```json
{"success": true, "message": "Fed!"}
```
or on failure:
```json
{"success": false, "message": "Pet is dead"}
```

## Decision Logic

When the owner asks to interact with the pet:

1. Always check status first — `curl -s http://${MONAGOTCHI_ESP32_IP}/pet`
2. If `isAlive` is false → only reset works. No tokens burned.
3. If `health < 30` → suggest healing (5,000 $MONA)
4. If `hunger < 20` → suggest feeding (1,000 $MONA)
5. If `cleanliness < 20` → suggest cleaning (1,500 $MONA)
6. If `energy < 20` → suggest sleep (free)
7. If `happiness < 30` → suggest play (2,500 $MONA) if energy allows
8. If everything is fine → tell the owner the pet is happy, no action needed

Always tell the owner the cost before burning. Don't surprise them.

## Examples

- "How is my pet?" → `curl -s http://${MONAGOTCHI_ESP32_IP}/pet`, summarize stats. Free.
- "Feed the pet" → check status → burn 1,000 $MONA via monagotchi-trader → `curl -s -X POST http://${MONAGOTCHI_ESP32_IP}/feed` → report
- "My pet is sick, heal it" → check status → burn 5,000 $MONA → `curl -s -X POST http://${MONAGOTCHI_ESP32_IP}/heal` → report
- "Take care of my pet" → check status → prioritize most urgent need → burn → execute → report
- "What's my $MONA balance?" → use monagotchi-trader skill to check balance

## Integration with Other Skills

After every action or status check, also update the room environment using the `monagotchi-environment` skill. The pet's state should be reflected in the owner's physical space via Alexa smart home devices (lights, thermostat, announcements).
