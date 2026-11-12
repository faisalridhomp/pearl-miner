/* Pearl (zhenzhu) PoW miner: proof = keccak256(abi.encode(bytes32 seed, uint256 nonce)) <= target.
 * Input is 64 bytes: seed (32) + nonce as 32-byte big-endian. Keccak-256 padding (0x01 ... 0x80).
 * Usage: pearl-miner <seed_hex64> <bits> [start_nonce] [threads]
 * Prints SOLVED nonce + digest + bits. Adapted from chip-miner (same keccak-f1600 core). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

static const uint64_t RC[24] = {
  0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
  0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
  0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
  0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
  0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
  0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
  0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
  0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};
static const int R[5][5] = {
  {0, 36, 3, 41, 18},
  {1, 44, 10, 45, 2},
  {62, 6, 43, 15, 61},
  {28, 55, 25, 21, 56},
  {27, 20, 39, 8, 14}
};
static inline uint64_t rotl(uint64_t x, int n){ return n==0?x:((x<<n)|(x>>(64-n))); }

static void keccak_f1600(uint64_t st[25]){
  for(int round=0; round<24; round++){
    uint64_t C[5], D[5], B[25];
    for(int x=0;x<5;x++) C[x]=st[x]^st[x+5]^st[x+10]^st[x+15]^st[x+20];
    for(int x=0;x<5;x++) D[x]=C[(x+4)%5]^rotl(C[(x+1)%5],1);
    for(int x=0;x<5;x++) for(int y=0;y<5;y++) st[x+5*y]^=D[x];
    for(int x=0;x<5;x++) for(int y=0;y<5;y++) B[y+5*((2*x+3*y)%5)]=rotl(st[x+5*y], R[x][y]);
    for(int x=0;x<5;x++) for(int y=0;y<5;y++) st[x+5*y]=B[x+5*y]^((~B[(x+1)%5+5*y])&B[(x+2)%5+5*y]);
    st[0]^=RC[round];
  }
}
static void keccak256_64(const unsigned char *in, unsigned char *out){
  uint64_t st[25]={0};
  unsigned char block[136]={0};
  memcpy(block, in, 64);
  block[64]=0x01;
  block[135]|=0x80;
  for(int i=0;i<17;i++){
    uint64_t lane=0;
    for(int b=0;b<8;b++) lane |= (uint64_t)block[i*8+b] << (8*b);
    st[i]=lane;
  }
  keccak_f1600(st);
  for(int i=0;i<4;i++) for(int b=0;b<8;b++) out[i*8+b]=(unsigned char)(st[i]>>(8*b));
}
static int leading_zero_bits(const unsigned char *d){
  int n=0;
  for(int i=0;i<32;i++){
    unsigned char byte=d[i];
    if(byte==0){ n+=8; continue; }
    while((byte & 0x80)==0){ n++; byte <<= 1; }
    break;
  }
  return n;
}
static unsigned char seed_bytes[32];
static int bits_needed;
static atomic_int found_flag = 0;
static uint64_t found_nonce = 0;
static unsigned char found_digest[32];

typedef struct { int id; int nthreads; uint64_t start; } arg_t;

static void *worker(void *p){
  arg_t *a=(arg_t*)p;
  unsigned char in[64];
  memcpy(in, seed_bytes, 32);
  uint64_t nonce = a->start + (uint64_t)a->id;
  uint64_t tried=0;
  while(!atomic_load(&found_flag)){
    memset(in+32, 0, 32);
    for(int b=0;b<8;b++) in[32+24+b]=(unsigned char)(nonce >> (56-8*b));
    unsigned char digest[32];
    keccak256_64(in, digest);
    tried++;
    if(leading_zero_bits(digest) >= bits_needed){
      if(atomic_exchange(&found_flag, 1)==0){
        found_nonce=nonce;
        memcpy(found_digest, digest, 32);
      }
      break;
    }
    nonce += (uint64_t)a->nthreads;
    if((tried % 50000000ULL)==0){
      fprintf(stderr,"worker %d tried %llu last_nonce %llu\n", a->id, (unsigned long long)tried, (unsigned long long)nonce);
      fflush(stderr);
    }
  }
  return NULL;
}
static int hexval(char c){
  if(c>='0'&&c<='9') return c-'0';
  if(c>='a'&&c<='f') return c-'a'+10;
  if(c>='A'&&c<='F') return c-'A'+10;
  return -1;
}
int main(int argc, char **argv){
  if(argc<3){
    fprintf(stderr,"usage: pearl-miner <seed_hex64> <bits> [start_nonce] [threads]\n");
    return 1;
  }
  const char *seed=argv[1];
  if(seed[0]=='0'&&(seed[1]=='x'||seed[1]=='X')) seed+=2;
  if(strlen(seed)!=64){ fprintf(stderr,"seed must be 64 hex chars\n"); return 1; }
  for(int i=0;i<32;i++) seed_bytes[i]=(unsigned char)((hexval(seed[2*i])<<4)|hexval(seed[2*i+1]));
  bits_needed=atoi(argv[2]);
  uint64_t start = argc>=4 ? strtoull(argv[3],NULL,10) : 0;
  int nthreads = argc>=5 ? atoi(argv[4]) : 4;
  if(nthreads<1) nthreads=1;
  if(nthreads>16) nthreads=16;
  pthread_t *th=malloc(sizeof(pthread_t)*nthreads);
  arg_t *args=malloc(sizeof(arg_t)*nthreads);
  for(int i=0;i<nthreads;i++){ args[i].id=i; args[i].nthreads=nthreads; args[i].start=start; pthread_create(&th[i],NULL,worker,&args[i]); }
  for(int i=0;i<nthreads;i++) pthread_join(th[i],NULL);
  printf("SOLVED nonce: %llu\n", (unsigned long long)found_nonce);
  printf("digest: ");
  for(int i=0;i<32;i++) printf("%02x", found_digest[i]);
  printf("\nbits: %d\n", leading_zero_bits(found_digest));
  return 0;
}
