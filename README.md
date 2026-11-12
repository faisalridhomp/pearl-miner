# pearl-miner

Proof-of-work miner in C for the Pearl PoW mint on BNB Chain.

Pearl is a daily PoW mint: each day a new seed is published, and miners
search for a nonce whose hash beats the day's target. This miner implements
the search with tiny-keccak (Keccak-256) in C.

## Build

```bash
gcc -O3 -o pearl-miner pearl-miner.c
```

## Usage

```bash
./pearl-miner <seed> <target> [start-nonce] [end-nonce]
```

The miner prints any nonce that satisfies the target, along with its hash.
Verified nonces are submitted on-chain with the mint contract's
`mineBatch` call (see the project's mint guide).

## Notes

- Single-threaded C, ~2 minutes per nonce at the day-10 difficulty on a
  shared 1.5-core VM. For production use, run multiple instances over
  disjoint nonce ranges.
- The 5 mining boxes sometimes referenced in older notes (omp, ore1, ore2,
  ore3, ticko) belong to someone else and are not part of this repo.
