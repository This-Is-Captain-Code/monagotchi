# Monagotchi

## Overview
ESP32 Tamagotchi pet powered by $MONA tokens on Monad blockchain, with Alexa smart home integration via OpenClaw.

## Project Structure
- `monagotchi.ino` — ESP32 firmware (Arduino). Flash via Arduino IDE.
- `monagotchi-controller/SKILL.md` — OpenClaw skill for controlling the pet via HTTP API
- `monagotchi-trader/SKILL.md` — OpenClaw skill for burning $MONA tokens on-chain
- `monagotchi-trader/scripts/monagotchi-trader.mjs` — Node.js script for ERC-20 token operations
- `monagotchi-trader/references/nadfun-contracts.md` — Monad contract addresses
- `monagotchi-environment/SKILL.md` — OpenClaw skill for Alexa environment mapping

## Key Decisions
- This is NOT a web app — it's firmware + OpenClaw skills for a physical device
- The trader script uses ethers.js v6 for Monad ERC-20 interactions
- Skills use OpenClaw SKILL.md frontmatter format with proper `metadata.openclaw.env` placement

## Recent Changes
- Fixed SKILL.md metadata format: moved `env` from `requires` to `metadata.openclaw.env`
- Added missing `MONAD_RPC_URL` to monagotchi-trader env requirements
- Created monagotchi-trader.mjs script implementing balance, burn, and info commands
- Created nadfun-contracts.md reference with Monad mainnet addresses
- Renamed nadfun-trader → monagotchi-trader for consistent naming across README and skills
