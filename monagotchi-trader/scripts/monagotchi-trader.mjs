#!/usr/bin/env node
import { ethers } from "ethers";

const BURN_ADDRESS = "0x000000000000000000000000000000000000dEaD";

const ACTION_COSTS = {
  feed: 1000,
  play: 2500,
  clean: 1500,
  heal: 5000,
};

const ERC20_ABI = [
  "function balanceOf(address) view returns (uint256)",
  "function transfer(address to, uint256 amount) returns (bool)",
  "function decimals() view returns (uint8)",
  "function name() view returns (string)",
  "function symbol() view returns (string)",
  "function totalSupply() view returns (uint256)",
];

function getConfig() {
  const tokenAddress = process.env.MONAGOTCHI_TOKEN_ADDRESS;
  const privateKey = process.env.MONAD_PRIVATE_KEY;
  const rpcUrl = process.env.MONAD_RPC_URL;

  if (!tokenAddress) {
    console.error("MONAGOTCHI_TOKEN_ADDRESS not set");
    process.exit(1);
  }
  if (!privateKey) {
    console.error("MONAD_PRIVATE_KEY not set");
    process.exit(1);
  }
  if (!rpcUrl) {
    console.error("MONAD_RPC_URL not set");
    process.exit(1);
  }

  return { tokenAddress, privateKey, rpcUrl };
}

function getProvider(rpcUrl) {
  return new ethers.JsonRpcProvider(rpcUrl);
}

function getWallet(privateKey, provider) {
  return new ethers.Wallet(privateKey, provider);
}

function getTokenContract(tokenAddress, signerOrProvider) {
  return new ethers.Contract(tokenAddress, ERC20_ABI, signerOrProvider);
}

async function checkBalance() {
  const { tokenAddress, privateKey, rpcUrl } = getConfig();
  const provider = getProvider(rpcUrl);
  const wallet = getWallet(privateKey, provider);
  const token = getTokenContract(tokenAddress, wallet);

  const decimals = await token.decimals();
  const rawBalance = await token.balanceOf(wallet.address);
  const balance = parseFloat(ethers.formatUnits(rawBalance, decimals));

  const affordable = {};
  for (const [action, cost] of Object.entries(ACTION_COSTS)) {
    affordable[action] = balance >= cost;
  }

  const result = {
    success: true,
    address: wallet.address,
    balance: balance.toString(),
    affordable,
  };

  console.log(JSON.stringify(result));
}

async function burnTokens(action) {
  const cost = ACTION_COSTS[action];
  if (!cost) {
    console.log(JSON.stringify({ success: false, error: `Unknown action: ${action}. Valid: ${Object.keys(ACTION_COSTS).join(", ")}` }));
    process.exit(1);
  }

  const { tokenAddress, privateKey, rpcUrl } = getConfig();
  const provider = getProvider(rpcUrl);
  const wallet = getWallet(privateKey, provider);
  const token = getTokenContract(tokenAddress, wallet);

  const decimals = await token.decimals();
  const rawBalance = await token.balanceOf(wallet.address);
  const balance = parseFloat(ethers.formatUnits(rawBalance, decimals));

  if (balance < cost) {
    console.log(JSON.stringify({ success: false, error: `Insufficient $MONA. Need ${cost}, have ${balance}` }));
    process.exit(1);
  }

  console.error(`Burning ${cost} $MONA for ${action}...`);

  const amount = ethers.parseUnits(cost.toString(), decimals);
  const tx = await token.transfer(BURN_ADDRESS, amount);
  console.error(`Tx sent: ${tx.hash}`);

  const receipt = await tx.wait();
  console.error(`Confirmed in block ${receipt.blockNumber}`);

  const newRawBalance = await token.balanceOf(wallet.address);
  const newBalance = parseFloat(ethers.formatUnits(newRawBalance, decimals));

  console.log(JSON.stringify({
    success: true,
    action,
    burned: cost,
    txHash: tx.hash,
    remainingBalance: newBalance.toString(),
  }));
}

async function getTokenInfo() {
  const { tokenAddress, privateKey, rpcUrl } = getConfig();
  const provider = getProvider(rpcUrl);
  const wallet = getWallet(privateKey, provider);
  const token = getTokenContract(tokenAddress, wallet);

  const [name, symbol, decimals, totalSupply, rawBalance] = await Promise.all([
    token.name(),
    token.symbol(),
    token.decimals(),
    token.totalSupply(),
    token.balanceOf(wallet.address),
  ]);

  const result = {
    success: true,
    name,
    symbol,
    decimals: Number(decimals),
    totalSupply: ethers.formatUnits(totalSupply, decimals),
    ownerBalance: ethers.formatUnits(rawBalance, decimals),
    tokenAddress,
    ownerAddress: wallet.address,
  };

  console.log(JSON.stringify(result));
}

const [command, arg] = process.argv.slice(2);

switch (command) {
  case "balance":
    await checkBalance();
    break;
  case "burn":
    if (!arg) {
      console.log(JSON.stringify({ success: false, error: "Usage: monagotchi-trader.mjs burn <feed|play|clean|heal>" }));
      process.exit(1);
    }
    await burnTokens(arg);
    break;
  case "info":
    await getTokenInfo();
    break;
  default:
    console.log(JSON.stringify({ success: false, error: "Usage: monagotchi-trader.mjs <balance|burn|info>" }));
    process.exit(1);
}
