# pearl-miner

Proof-of-work miner in C for the Pearl PoW mint on BNB Chain.

Pearl is a daily PoW mint: each day a new seed is published, and miners
search for a nonce whose hash beats the day's target. This miner implements
the search with tiny-keccak (Keccak-256) in C.

## Build

```bash
gcc -O3 -o pearl-miner pearl-miner.c
```

No dependencies beyond a C compiler and pthreads (linked from libc on
modern systems; add `-lpthread` if your linker complains).

## Usage

```bash
./pearl-miner <seed_hex64> <bits> [start_nonce] [threads]
```

- `seed_hex64` — the day's 64-hex-char seed (with or without `0x` prefix)
- `bits` — required leading zero bits in the keccak256 digest
- `start_nonce` — first nonce to try (default 0)
- `threads` — worker threads, 1–16 (default 4)

The miner prints the first nonce whose digest meets the target, plus the
digest and its bit count. Verified nonces are submitted on-chain with the
mint contract's `mineBatch` call (see the project's mint guide).

Example (easy target, finds a nonce in under a second):

```bash
./pearl-miner 0000000000000000000000000000000000000000000000000000000000000000 8 0 1
# SOLVED nonce: 50
# digest: 00bcd6ff29ae71d399fb597d99792fa72d0863bd723b9ab11f79d0b8d8ac5bc8
# bits: 8
```

The digest is plain `keccak256(seed || nonce)` where the nonce is a
32-byte big-endian integer — verifiable with any keccak library.

## Notes

- Multi-threaded (pthreads): nonce ranges are split across workers, so
  more threads = proportionally faster. On a shared 1.5-core VM, 4
  threads is about right; on a real machine use all cores.
- The 5 mining boxes sometimes referenced in older notes (omp, ore1, ore2,
  ore3, ticko) belong to someone else and are not part of this repo.
