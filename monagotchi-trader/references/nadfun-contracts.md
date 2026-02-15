# Nad.fun Contract Addresses (Monad Mainnet)

## Core Contracts

| Contract | Address |
|----------|---------|
| Bonding Curve | `0xA7283d07812a02AFB7C09B60f8896bCEA3F90aCE` |
| Lens | `0x7e78A8DE94f21804F7a17F4E8BF9EC2c872187ea` |
| Burn Address | `0x000000000000000000000000000000000000dEaD` |

## Bonding Curve ABI (Key Functions)

```solidity
function buy(address token) external payable
function sell(address token, uint256 amount, uint256 minPayout) external
function getTokenInfo(address token) external view returns (TokenInfo memory)
```

## Lens ABI (Key Functions)

```solidity
function getTokenOverview(address token) external view returns (Overview memory)
function getUserTokens(address user) external view returns (address[] memory)
```

## ERC-20 Token ABI (For $MONA)

```solidity
function balanceOf(address account) external view returns (uint256)
function transfer(address to, uint256 amount) external returns (bool)
function approve(address spender, uint256 amount) external returns (bool)
function allowance(address owner, address spender) external view returns (uint256)
function name() external view returns (string memory)
function symbol() external view returns (string memory)
function decimals() external view returns (uint8)
function totalSupply() external view returns (uint256)
```

## Notes

- All tokens on nad.fun follow ERC-20 standard
- Burning = transferring to `0x...dEaD` (standard burn pattern)
- Gas is paid in MON (Monad native token)
- Token addresses are created when launched on nad.fun
