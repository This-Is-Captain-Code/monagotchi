---
name: monagotchi-environment
description: Map Monagotchi pet state to real-world environment via Alexa smart home devices. Use after any pet action or status check to adjust room lights, temperature, and announce status. Requires the alexa-cli skill (buddyh/alexa-cli). Works with monagotchi-controller.
metadata: {"openclaw": {"emoji": "🏠", "requires": {"bins": ["alexacli"], "env": ["MONAGOTCHI_ALEXA_DEVICE"]}}}
---

# Monagotchi Environment Controller

Your Monagotchi controls your room. When the pet is sick, your lights go red and the room heats up. When it's happy, everything is calm and green. Neglect your pet and your environment punishes you.

## How It Works

After every pet status check or action, evaluate the pet's state and adjust the room accordingly using `alexacli`. The pet's wellbeing is reflected in your physical space — you can't ignore a dying pet when your room is red and 85°F.

## Prerequisites

Install the alexa-cli skill from ClawHub:
```bash
clawhub install buddyh/alexa-cli
```

Make sure `alexacli` is set up and authenticated with your Amazon account. Test with:
```bash
alexacli devices
```

## Configuration

Required:
```
MONAGOTCHI_ALEXA_DEVICE=Bedroom    # Which Echo/room to control
```

The device name supports partial, case-insensitive matching (same as alexacli).

## State-to-Environment Mapping

### 🟢 normal / happy (happiness > 50)
The pet is doing well. Room returns to baseline.

```bash
alexacli command "set lights to green" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli command "set thermostat to 72" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli speak "Monagotchi is happy and healthy." -d ${MONAGOTCHI_ALEXA_DEVICE}
```

### 🟡 hungry (hunger < 20)
Pet needs food. Room goes warm yellow — a gentle nudge.

```bash
alexacli command "set lights to yellow" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli command "set thermostat to 76" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli speak "Monagotchi is getting hungry. Feed it soon." -d ${MONAGOTCHI_ALEXA_DEVICE}
```

### 🟠 low energy (energy < 20) / sleeping
Pet is exhausted or resting. Room dims down.

```bash
alexacli command "dim lights to 20 percent" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli command "set lights to blue" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli command "set thermostat to 70" -d ${MONAGOTCHI_ALEXA_DEVICE}
```

No announcement when sleeping — let it rest.

### 🔴 sick (health < 30)
Pet is in trouble. Room goes red, temperature rises. You need to act.

```bash
alexacli command "set lights to red" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli command "set thermostat to 80" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli speak "Warning. Monagotchi is sick. It needs healing." -d ${MONAGOTCHI_ALEXA_DEVICE}
```

### 💀 dead (isAlive = false)
Pet died. Room goes full red, hot, alarm mode.

```bash
alexacli command "set lights to red" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli command "set lights brightness to 100 percent" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli command "set thermostat to 85" -d ${MONAGOTCHI_ALEXA_DEVICE}
alexacli speak "Monagotchi has died. Your room will stay like this until you reset." -d ${MONAGOTCHI_ALEXA_DEVICE}
```

### ✨ action just taken (feeding, playing, etc.)
Pet was just cared for. Brief positive feedback — flash green, then return to the state-appropriate color.

```bash
alexacli command "set lights to green" -d ${MONAGOTCHI_ALEXA_DEVICE}
```

## Decision Logic

After every interaction with the pet (via monagotchi-controller), or on periodic status checks:

1. Get pet status from ESP32: `curl -s http://${MONAGOTCHI_ESP32_IP}/pet`
2. Determine the environment state based on priority:
   - `isAlive == false` → 💀 dead (highest priority)
   - `state == "sick"` or `health < 30` → 🔴 sick
   - `state == "hungry"` or `hunger < 20` → 🟡 hungry
   - `state == "sleeping"` → 🟠 sleeping
   - `happiness > 50` → 🟢 happy
   - else → 🟢 normal (default baseline)
3. Execute the corresponding `alexacli` commands
4. Only announce via speech if the state **changed** since last check — don't repeat yourself

## Commands Reference

```bash
# List available Alexa devices
alexacli devices

# Set light color
alexacli command "set lights to COLOR" -d DEVICE

# Set brightness
alexacli command "set lights brightness to PERCENT percent" -d DEVICE

# Set thermostat
alexacli command "set thermostat to TEMP" -d DEVICE

# Text-to-speech on specific device
alexacli speak "MESSAGE" -d DEVICE

# Announce on ALL devices
alexacli speak "MESSAGE" --announce
```

## Examples

- Pet status shows `health: 25, state: "sick"` → `alexacli command "set lights to red" -d Bedroom` → `alexacli command "set thermostat to 80" -d Bedroom` → `alexacli speak "Warning. Monagotchi is sick." -d Bedroom`
- Owner heals the pet (burns 5,000 $MONA) → flash green, then set to calm green, thermostat to 72
- Pet dies → lights full red, thermostat 85, announce death. Stays until reset.
- Owner says "how is my pet?" → check status, report stats, update environment to match
- Owner resets pet → `alexacli command "set lights to green" -d Bedroom` → `alexacli command "set thermostat to 72" -d Bedroom` → `alexacli speak "Monagotchi is reborn!" -d Bedroom`
