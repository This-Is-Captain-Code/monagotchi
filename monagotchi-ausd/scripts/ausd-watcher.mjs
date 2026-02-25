// ausd-watcher.mjs
// Watches for AUSD transfers to the Monagotchi treasury on Monad.
// Logs wallet, amount, and tx hash to ausd_log.json for Moltiverse/Agora tracking.

import { ethers } from "ethers";
import { readFileSync, writeFileSync, existsSync } from "fs";
import { fileURLToPath } from "url";
import { dirname, join } from "path";

const __dirname = dirname(fileURLToPath(import.meta.url));
const LOG_FILE = join(__dirname, "ausd_log.json");

// --- Config ---
const AUSD_ADDRESS = "0x00000000eFE302BEAA2b3e6e1b18d08D69a9012a";
const AUSD_DECIMALS = 6;
const TREASURY_ADDRESS = process.env.TREASURY_ADDRESS;
const RPC_URL = process.env.MONAD_RPC_URL;

if (!TREASURY_ADDRESS || !RPC_URL) {
  console.error("❌ Missing TREASURY_ADDRESS or MONAD_RPC_URL env vars");
  process.exit(1);
}

const ERC20_ABI = [
  "event Transfer(address indexed from, address indexed to, uint256 amount)"
];

// --- Log helpers ---
function readLog() {
  if (!existsSync(LOG_FILE)) return [];
  try {
    return JSON.parse(readFileSync(LOG_FILE, "utf8"));
  } catch {
    return [];
  }
}

function appendLog(entry) {
  const log = readLog();
  log.push(entry);
  writeFileSync(LOG_FILE, JSON.stringify(log, null, 2));
}

// --- Main ---
async function main() {
  const provider = new ethers.JsonRpcProvider(RPC_URL);
  const ausd = new ethers.Contract(AUSD_ADDRESS, ERC20_ABI, provider);

  console.log("🪙 AUSD watcher started");
  console.log(`   Treasury: ${TREASURY_ADDRESS}`);
  console.log(`   Logging to: ${LOG_FILE}`);

  ausd.on("Transfer", async (from, to, amount, event) => {
    try {
      if (to.toLowerCase() !== TREASURY_ADDRESS.toLowerCase()) return;

      const amountFormatted = (
        Number(amount) / Math.pow(10, AUSD_DECIMALS)
      ).toFixed(2);

      const entry = {
        wallet: from,
        amount: amount.toString(),
        amountFormatted,
        txHash: event.log.transactionHash,
        timestamp: new Date().toISOString()
      };

      console.log(`💰 AUSD received: ${amountFormatted} AUSD from ${from}`);
      console.log(`   tx: ${entry.txHash}`);

      appendLog(entry);
      console.log(`✅ Logged to ${LOG_FILE}`);
    } catch (err) {
      console.error("Watcher error:", err);
    }
  });

  // Keep alive
  provider.on("error", (err) => {
    console.error("Provider error:", err);
  });
}

main().catch(console.error);
