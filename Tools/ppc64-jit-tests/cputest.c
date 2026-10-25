// A test program for Dolphin's CPU cores. It runs many instructions on pseudo-random and edge
// case inputs, and reports a hash of every result, XER, CR and FPSCR value for each group of
// tests, so that running it on two CPU cores and comparing the reports finds any difference.
//
// Reports go through OSReport, which Dolphin replaces with a printf to its log in debug mode.

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef unsigned long long u64;

// noipa: Dolphin reads the arguments, so the compiler must not assume that this function doesn't.
void OSReport(const char* format, ...) __attribute__((noipa, used));
void OSReport(const char* format, ...)
{
  asm volatile("" : : : "memory");
}

static u32 rng_state = 0x12345678;
static u32 Random(void)
{
  u32 x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return rng_state = x;
}

// Edge cases half of the time, random values otherwise.
static u32 Value(void)
{
  static const u32 edge[] = {0,      1,      2,       0x7FFFFFFF, 0x80000000, 0xFFFFFFFF,
                             0xFFFFFFFE, 0x8000, 0x7FFF, 0xFFFF,     0x10000,    31,
                             32,     33,     0x80000001, 0x55555555};
  const u32 r = Random();
  return (r & 1) ? Random() : edge[(r >> 1) % (sizeof(edge) / sizeof(edge[0]))];
}

static u32 hash;
// Each instruction of a group also has its own hash, so that a difference names the instruction.
#define MAX_OPS 128
static u32 op_hashes[MAX_OPS];
static const char* op_names[MAX_OPS];
static u32 op_index, op_count;

static void Begin(void)
{
  hash = 0x811C9DC5;
  for (int i = 0; i < MAX_OPS; i++)
    op_hashes[i] = 0x811C9DC5;
  op_count = 0;
}
static void Mix(u32 value)
{
  hash ^= value;
  hash *= 0x01000193;
  hash ^= hash >> 15;
}
static void Mix64(u64 value)
{
  Mix((u32)(value >> 32));
  Mix((u32)value);
}
static void BeginIteration(void)
{
  op_index = 0;
}
static void Record(const char* name, u32 a, u32 b, u32 c)
{
  u32 h = op_hashes[op_index];
  const u32 values[3] = {a, b, c};
  for (int i = 0; i < 3; i++)
  {
    h ^= values[i];
    h *= 0x01000193;
    h ^= h >> 15;
  }
  op_hashes[op_index] = h;
  op_names[op_index] = name;
  op_index++;
  if (op_index > op_count)
    op_count = op_index;
  Mix(a);
  Mix(b);
  Mix(c);
}
static void Record64(const char* name, u64 a, u64 b, u32 c)
{
  Record(name, (u32)(a >> 32) ^ (u32)a * 3, (u32)(b >> 32) ^ (u32)b * 5, c);
  Mix64(a);
  Mix64(b);
}
static void End(const char* name)
{
  OSReport("TEST %s %08x\n", name, hash);
  for (u32 i = 0; i < op_count; i++)
  {
    // The mnemonic, up to the first space.
    char mnemonic[16];
    u32 length = 0;
    while (length < sizeof(mnemonic) - 1 && op_names[i][length] && op_names[i][length] != ' ')
    {
      mnemonic[length] = op_names[i][length];
      length++;
    }
    mnemonic[length] = 0;
    OSReport("TEST %s/%02u/%s %08x\n", name, i, mnemonic, op_hashes[i]);
  }
}

#define ITERATIONS 2000

// Integer instructions, with XER and CR captured afterwards.
#define OP2(insn)                                                                                  \
  do                                                                                               \
  {                                                                                                \
    u32 r_, x_, c_;                                                                                \
    asm volatile("mtxer %5\n\t" insn " %0,%3,%4\n\tmfxer %1\n\tmfcr %2"                            \
                 : "=&r"(r_), "=&r"(x_), "=&r"(c_)                                                 \
                 : "r"(a), "r"(b), "r"(xer)                                                        \
                 : "cr0", "xer");                                                                  \
    Record(insn, r_, x_, c_ >> 28);                                                                \
  } while (0)
#define OP1(insn)                                                                                  \
  do                                                                                               \
  {                                                                                                \
    u32 r_, x_, c_;                                                                                \
    asm volatile("mtxer %4\n\t" insn " %0,%3\n\tmfxer %1\n\tmfcr %2"                               \
                 : "=&r"(r_), "=&r"(x_), "=&r"(c_)                                                 \
                 : "r"(a), "r"(xer)                                                                \
                 : "cr0", "xer");                                                                  \
    Record(insn, r_, x_, c_ >> 28);                                                                \
  } while (0)
#define OPI(insn, imm) OP1(insn " %0,%3," #imm "\n\t#")
#define RLWIMI(sh, mb, me)                                                                         \
  do                                                                                               \
  {                                                                                                \
    u32 r_ = b, c_;                                                                                \
    asm volatile("rlwimi. %0,%2," #sh "," #mb "," #me "\n\tmfcr %1"                                \
                 : "+r"(r_), "=&r"(c_)                                                             \
                 : "r"(a)                                                                          \
                 : "cr0");                                                                         \
    Record("rlwimi.", r_, c_ >> 28, 0);                                                            \
  } while (0)

static void TestIntegerArithmetic(void)
{
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    const u32 a = Value(), b = Value();
    const u32 xer = Random() & 0xE000FFFF;  // SO, OV, CA, the string length and compare byte
    OP2("add");
    OP2("add.");
    OP2("addo");
    OP2("addo.");
    OP2("addc");
    OP2("addc.");
    OP2("addco");
    OP2("adde");
    OP2("adde.");
    OP2("addeo.");
    OP2("subf");
    OP2("subf.");
    OP2("subfo.");
    OP2("subfc");
    OP2("subfco.");
    OP2("subfe");
    OP2("subfe.");
    OP2("subfeo");
    OP2("mullw");
    OP2("mullw.");
    OP2("mullwo.");
    OP2("mulhw");
    OP2("mulhw.");
    OP2("mulhwu");
    OP2("mulhwu.");
    OP2("divw");
    OP2("divw.");
    OP2("divwo.");
    OP2("divwu");
    OP2("divwu.");
    OP2("divwuo.");
    OP1("neg");
    OP1("neg.");
    OP1("nego.");
    OP1("addze");
    OP1("addze.");
    OP1("addzeo");
    OP1("addme");
    OP1("addme.");
    OP1("addmeo.");
    OP1("subfze.");
    OP1("subfzeo");
    OP1("subfme.");
    OP1("subfmeo");
    OPI("addi", -5);
    OPI("addis", 0x1234);
    OPI("addic", -1);
    OPI("addic.", 100);
    OPI("subfic", 7);
    OPI("subfic", -32768);
    OPI("mulli", -3);
    OPI("mulli", 32767);
  }
  End("integer-arithmetic");
}

// Every form of the XO-form instructions: with and without Rc and OE, whose bit is part of the
// extended opcode, and with the destination the same as a source, as the overflow is computed
// before the destination is written.
#define OP2_DA(insn)                                                                               \
  do                                                                                               \
  {                                                                                                \
    u32 r_ = a, x_, c_;                                                                            \
    asm volatile("mtxer %4\n\t" insn " %0,%0,%3\n\tmfxer %1\n\tmfcr %2"                            \
                 : "+r"(r_), "=&r"(x_), "=&r"(c_)                                                  \
                 : "r"(b), "r"(xer)                                                                \
                 : "cr0", "xer");                                                                  \
    Record(insn " d=a", r_, x_, c_ >> 28);                                                         \
  } while (0)
#define OP2_DB(insn)                                                                               \
  do                                                                                               \
  {                                                                                                \
    u32 r_ = b, x_, c_;                                                                            \
    asm volatile("mtxer %4\n\t" insn " %0,%3,%0\n\tmfxer %1\n\tmfcr %2"                            \
                 : "+r"(r_), "=&r"(x_), "=&r"(c_)                                                  \
                 : "r"(a), "r"(xer)                                                                \
                 : "cr0", "xer");                                                                  \
    Record(insn " d=b", r_, x_, c_ >> 28);                                                         \
  } while (0)
#define OP1_DA(insn)                                                                               \
  do                                                                                               \
  {                                                                                                \
    u32 r_ = a, x_, c_;                                                                            \
    asm volatile("mtxer %3\n\t" insn " %0,%0\n\tmfxer %1\n\tmfcr %2"                               \
                 : "+r"(r_), "=&r"(x_), "=&r"(c_)                                                  \
                 : "r"(xer)                                                                        \
                 : "cr0", "xer");                                                                  \
    Record(insn " d=a", r_, x_, c_ >> 28);                                                         \
  } while (0)

static void TestIntegerForms(void)
{
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    const u32 a = Value(), b = Value();
    const u32 xer = Random() & 0xE000007F;
    OP2("add");
    OP2("add.");
    OP2("addo");
    OP2("addo.");
    OP2("addc");
    OP2("addc.");
    OP2("addco");
    OP2("addco.");
    OP2("adde");
    OP2("adde.");
    OP2("addeo");
    OP2("addeo.");
    OP2("subf");
    OP2("subf.");
    OP2("subfo");
    OP2("subfo.");
    OP2("subfc");
    OP2("subfc.");
    OP2("subfco");
    OP2("subfco.");
    OP2("subfe");
    OP2("subfe.");
    OP2("subfeo");
    OP2("subfeo.");
    OP2("mullw");
    OP2("mullw.");
    OP2("mullwo");
    OP2("mullwo.");
    OP2("divw");
    OP2("divw.");
    OP2("divwo");
    OP2("divwo.");
    OP2("divwu");
    OP2("divwu.");
    OP2("divwuo");
    OP2("divwuo.");
    OP1("neg");
    OP1("neg.");
    OP1("nego");
    OP1("nego.");
    OP1("addze");
    OP1("addze.");
    OP1("addzeo");
    OP1("addzeo.");
    OP1("addme");
    OP1("addme.");
    OP1("addmeo");
    OP1("addmeo.");
    OP1("subfze");
    OP1("subfze.");
    OP1("subfzeo");
    OP1("subfzeo.");
    OP1("subfme");
    OP1("subfme.");
    OP1("subfmeo");
    OP1("subfmeo.");
  }
  End("integer-forms");

  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    const u32 a = Value(), b = Value();
    const u32 xer = Random() & 0xE000007F;
    OP2_DA("add.");
    OP2_DB("add.");
    OP2_DA("addo.");
    OP2_DB("addo.");
    OP2_DA("addc.");
    OP2_DB("addc.");
    OP2_DA("addco.");
    OP2_DB("addco.");
    OP2_DA("adde.");
    OP2_DB("adde.");
    OP2_DA("addeo.");
    OP2_DB("addeo.");
    OP2_DA("subf.");
    OP2_DB("subf.");
    OP2_DA("subfo.");
    OP2_DB("subfo.");
    OP2_DA("subfc.");
    OP2_DB("subfc.");
    OP2_DA("subfco.");
    OP2_DB("subfco.");
    OP2_DA("subfe.");
    OP2_DB("subfe.");
    OP2_DA("subfeo.");
    OP2_DB("subfeo.");
    OP2_DA("mullw.");
    OP2_DB("mullw.");
    OP2_DA("mullwo.");
    OP2_DB("mullwo.");
    OP2_DA("divw.");
    OP2_DB("divw.");
    OP2_DA("divwo.");
    OP2_DB("divwo.");
    OP2_DA("divwu.");
    OP2_DB("divwu.");
    OP2_DA("divwuo.");
    OP2_DB("divwuo.");
    OP1_DA("neg.");
    OP1_DA("nego.");
    OP1_DA("addze.");
    OP1_DA("addzeo.");
    OP1_DA("addme.");
    OP1_DA("addmeo.");
    OP1_DA("subfze.");
    OP1_DA("subfzeo.");
    OP1_DA("subfme.");
    OP1_DA("subfmeo.");
  }
  End("integer-forms-aliased");
}

static void TestIntegerLogic(void)
{
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    const u32 a = Value(), b = Value();
    const u32 xer = Random() & 0xE000007F;
    OP2("and");
    OP2("and.");
    OP2("andc.");
    OP2("or");
    OP2("or.");
    OP2("orc.");
    OP2("xor.");
    OP2("nand.");
    OP2("nor.");
    OP2("eqv.");
    OP2("slw");
    OP2("slw.");
    OP2("srw.");
    OP2("sraw");
    OP2("sraw.");
    OP1("extsb.");
    OP1("extsh.");
    OP1("cntlzw");
    OP1("cntlzw.");
    OPI("andi.", 0xF0F0);
    OPI("andis.", 0x8001);
    OPI("ori", 0x8000);
    OPI("oris", 0xFFFF);
    OPI("xori", 0x1234);
    OPI("xoris", 0x8000);
    OPI("srawi", 0);
    OPI("srawi.", 5);
    OPI("srawi.", 31);
  }
  End("integer-logic");
}

#define RLWINM(sh, mb, me) OP1("rlwinm. %0,%3," #sh "," #mb "," #me "\n\t#")
#define RLWNM(mb, me) OP2("rlwnm. %0,%3,%4," #mb "," #me "\n\t#")

static void TestRotates(void)
{
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    const u32 a = Value(), b = Value();
    const u32 xer = 0;
    RLWINM(0, 0, 31);
    RLWINM(1, 0, 30);
    RLWINM(31, 1, 31);
    RLWINM(16, 16, 31);
    RLWINM(8, 24, 7);  // Wrapping mask
    RLWINM(5, 3, 3);
    RLWINM(0, 31, 0);
    RLWINM(27, 5, 29);
    RLWNM(0, 31);
    RLWNM(4, 27);
    RLWNM(28, 3);
    RLWIMI(0, 0, 31);
    RLWIMI(8, 16, 23);
    RLWIMI(24, 28, 3);
    RLWIMI(31, 0, 0);
  }
  End("rotates");
}

#define CMP(insn)                                                                                  \
  do                                                                                               \
  {                                                                                                \
    u32 c_;                                                                                        \
    asm volatile("mtcrf 0xff,%3\n\tmtxer %4\n\t" insn "\n\tmfcr %0"                                \
                 : "=&r"(c_)                                                                       \
                 : "r"(a), "r"(b), "r"(cr), "r"(xer)                                               \
                 : "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7", "xer");                 \
    Record(insn, c_, 0, 0);                                                                        \
  } while (0)

static void TestCompareAndConditionRegister(void)
{
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    const u32 a = Value(), b = Value();
    const u32 cr = Random();
    const u32 xer = Random() & 0xE0000000;
    CMP("cmpw 3,%1,%2");
    CMP("cmplw 5,%1,%2");
    CMP("cmpwi 1,%1,-1");
    CMP("cmplwi 7,%1,100");
    // Immediates at the ends of their ranges.
    CMP("cmpwi 2,%1,-32768");
    CMP("cmpwi 3,%1,32767");
    CMP("cmplwi 4,%1,0x8000");
    CMP("cmplwi 6,%1,0xffff");
    CMP("cmpw 0,%1,%2");
    CMP("crand 1,2,31");
    CMP("cror 4,9,30");
    CMP("crxor 5,5,5");
    CMP("crnand 6,7,8");
    CMP("crnor 31,0,15");
    CMP("creqv 12,12,12");
    CMP("crandc 20,21,22");
    CMP("crorc 25,26,27");
    CMP("mcrf 2,6");
    CMP("mcrxr 4");
    CMP("mtcrf 0x24,%1");
    CMP("mtcrf 0x81,%2");
  }
  End("compare-cr");
}

static u8 memory[256] __attribute__((aligned(32)));

static void FillMemory(void)
{
  for (u32 i = 0; i < sizeof(memory); i += 4)
    *(u32*)&memory[i] = Random();
}

static void MixMemory(void)
{
  for (u32 i = 0; i < sizeof(memory); i += 4)
    Mix(*(u32*)&memory[i]);
}

#define LOAD(insn, offset)                                                                         \
  do                                                                                               \
  {                                                                                                \
    u32 r_;                                                                                        \
    asm volatile(insn " %0," #offset "(%1)" : "=r"(r_) : "b"(base) : "memory");                    \
    Record(insn, r_, 0, 0);                                                                        \
  } while (0)
#define LOADX(insn)                                                                                \
  do                                                                                               \
  {                                                                                                \
    u32 r_;                                                                                        \
    asm volatile(insn " %0,%1,%2" : "=r"(r_) : "b"(memory), "r"(index) : "memory");                \
    Record(insn, r_, 0, 0);                                                                        \
  } while (0)
#define LOADU(insn, offset)                                                                        \
  do                                                                                               \
  {                                                                                                \
    u32 r_;                                                                                        \
    u8* p_ = base;                                                                                 \
    asm volatile(insn " %0," #offset "(%1)" : "=r"(r_), "+b"(p_) : : "memory");                    \
    Record(insn, r_, (u32)(p_ - memory), 0);                                                       \
  } while (0)
#define LOADUX(insn)                                                                               \
  do                                                                                               \
  {                                                                                                \
    u32 r_;                                                                                        \
    u8* p_ = memory;                                                                               \
    asm volatile(insn " %0,%1,%2" : "=&r"(r_), "+b"(p_) : "r"(index) : "memory");                  \
    Record(insn, r_, (u32)(p_ - memory), 0);                                                       \
  } while (0)
#define STOREU(insn, offset)                                                                       \
  do                                                                                               \
  {                                                                                                \
    u8* p_ = base;                                                                                 \
    asm volatile(insn " %1," #offset "(%0)" : "+b"(p_) : "r"(value) : "memory");                   \
    Record(insn, (u32)(p_ - memory), 0, 0);                                                        \
  } while (0)
#define STOREUX(insn)                                                                              \
  do                                                                                               \
  {                                                                                                \
    u8* p_ = memory;                                                                               \
    asm volatile(insn " %1,%0,%2" : "+b"(p_) : "r"(value), "r"(index) : "memory");                 \
    Record(insn, (u32)(p_ - memory), 0, 0);                                                        \
  } while (0)
#define STORE(insn, offset)                                                                        \
  asm volatile(insn " %0," #offset "(%1)" : : "r"(value), "b"(base) : "memory")
#define STOREX(insn) asm volatile(insn " %0,%1,%2" : : "r"(value), "b"(memory), "r"(index) : "memory")

static void TestLoadsAndStores(void)
{
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    FillMemory();
    u8* const base = &memory[64 + (Random() % 64)];
    const u32 index = Random() % 200;
    const u32 value = Value();

    LOAD("lbz", 0);
    LOAD("lbz", -1);
    LOAD("lhz", 3);
    LOAD("lha", 6);
    LOAD("lha", -7);
    LOAD("lwz", 0);
    LOAD("lwz", 13);
    LOADX("lbzx");
    LOADX("lhzx");
    LOADX("lhax");
    LOADX("lwzx");
    LOADX("lhbrx");
    LOADX("lwbrx");
    LOADU("lbzu", 1);
    LOADU("lhzu", -2);
    LOADU("lhau", 4);
    LOADU("lwzu", 8);
    LOADUX("lbzux");
    LOADUX("lhzux");
    LOADUX("lhaux");
    LOADUX("lwzux");

    STORE("stb", 0);
    STORE("sth", 5);
    STORE("stw", -9);
    STOREX("stbx");
    STOREX("sthx");
    STOREX("stwx");
    STOREX("sthbrx");
    MixMemory();
    STOREX("stwbrx");
    // The update forms store at the same addresses as the indexed ones.
    MixMemory();
    STOREU("stbu", 3);
    STOREU("sthu", -6);
    STOREU("stwu", 10);
    STOREUX("stbux");
    STOREUX("sthux");
    STOREUX("stwux");
    MixMemory();

    // Load and store multiple words and strings, on aligned addresses.
    u32 words[4];
    asm volatile("lmw 28,0(%4)\n\tmr %0,28\n\tmr %1,29\n\tmr %2,30\n\tmr %3,31"
                 : "=r"(words[0]), "=r"(words[1]), "=r"(words[2]), "=r"(words[3])
                 : "b"(&memory[16])
                 : "r28", "r29", "r30", "r31", "memory");
    for (int w = 0; w < 4; w++)
      Mix(words[w]);
    asm volatile("mr 28,%0\n\tmr 29,%1\n\tmr 30,%0\n\tmr 31,%1\n\tstmw 28,0(%2)"
                 :
                 : "r"(value), "r"(index), "b"(&memory[96])
                 : "r28", "r29", "r30", "r31", "memory");
    asm volatile("lswi 28,%4,13\n\tmr %0,28\n\tmr %1,29\n\tmr %2,30\n\tmr %3,31"
                 : "=r"(words[0]), "=r"(words[1]), "=r"(words[2]), "=r"(words[3])
                 : "b"(&memory[33])
                 : "r28", "r29", "r30", "r31", "memory");
    for (int w = 0; w < 4; w++)
      Mix(words[w]);
    asm volatile("mr 28,%0\n\tmr 29,%1\n\tstswi 28,%2,7" : : "r"(value), "r"(index), "b"(&memory[150])
                 : "r28", "r29", "memory");

    // Zero a cache block, and one through the uncached mirror, which the JIT zeroes through the MMU.
    asm volatile("dcbz 0,%0" : : "b"(&memory[128 + 32 * (Random() % 4)]) : "memory");
    asm volatile("dcbz %0,%1"
                 :
                 : "b"((u32)memory | 0x40000000), "r"(32 * (Random() % 8) + Random() % 32)
                 : "memory");
    MixMemory();
  }
  End("loads-stores");
}

// Accesses to MMIO, which the JIT's fastmem can't do: the first one faults, and later ones take
// the slow path that the fault handler patched in.
static void TestMMIO(void)
{
  Begin();
  // VI top field base (left), which reads back what was written to it.
  u8* const base = (u8*)0xCC00201C;
  const u32 saved = *(volatile u32*)base;
  for (int i = 0; i < 100; i++)
  {
    BeginIteration();
    u32 value = Random() & 0x00FFFE00;
    STORE("stw", 0);
    LOAD("lwz", 0);
    LOAD("lhz", 0);
    LOAD("lhz", 2);
    LOAD("lha", 2);
    value = Random() & 0xFE00;
    STORE("sth", 2);
    LOAD("lwz", 0);
  }
  *(volatile u32*)base = saved;
  End("mmio");
}

// Reservations, eieio, and segment registers.
static void TestReservationsAndSegments(void)
{
  Begin();
  static u32 word __attribute__((aligned(32)));
  for (int i = 0; i < 200; i++)
  {
    BeginIteration();
    word = Random();
    const u32 value = Value();
    // A reservation, a store conditional that succeeds, and one that fails without a reservation.
    u32 old, cr_stored, cr_failed;
    asm volatile("mtxer %5\n\tlwarx %0,0,%3\n\tadd %0,%0,%4\n\tstwcx. %0,0,%3\n\tmfcr %1\n\t"
                 "stwcx. %4,0,%3\n\tmfcr %2"
                 : "=&r"(old), "=&r"(cr_stored), "=&r"(cr_failed)
                 : "b"(&word), "r"(value), "r"(Random() & 0x80000000)
                 : "cr0", "xer", "memory");
    Record("lwarx", old, cr_stored >> 28, cr_failed >> 28);
    Mix(word);

    asm volatile("eieio");

    // Write a segment register and read it back, by number and by address, then restore it.
    u32 saved, by_number, by_address;
    asm volatile("mfsr %0,5\n\tmtsr 5,%3\n\tmfsr %1,5\n\tmfsrin %2,%4\n\tmtsr 5,%0"
                 : "=&r"(saved), "=&r"(by_number), "=&r"(by_address)
                 : "r"(Random() & 0x70FFFFFF), "r"(0x50000000 | (Random() & 0x0FFFFFFF)));
    Record("mtsr", by_number, by_address, 0);
  }
  End("reservations-segments");
}

static u32 Fibonacci(u32 n)
{
  return n < 2 ? n : Fibonacci(n - 1) + Fibonacci(n - 2);
}

static u32 F0(u32 x) { return x + 1; }
static u32 F1(u32 x) { return x * 3; }
static u32 F2(u32 x) { return x ^ 0x5A5A; }
static u32 F3(u32 x) { return x >> 3; }
static u32 (*const functions[])(u32) = {F0, F1, F2, F3};

static void TestBranches(void)
{
  Begin();

  // Calls and returns through LR.
  Mix(Fibonacci(22));

  // Indirect calls through CTR.
  u32 x = 1;
  for (int i = 0; i < 10000; i++)
    x = functions[Random() % 4](x);
  Mix(x);

  // A jump table.
  for (int i = 0; i < 10000; i++)
  {
    switch (Random() % 8)
    {
    case 0: x += 7; break;
    case 1: x ^= 0x1234; break;
    case 2: x *= 5; break;
    case 3: x -= 3; break;
    case 4: x = (x << 3) | (x >> 29); break;
    case 5: x += x >> 7; break;
    case 6: x = ~x; break;
    default: x++; break;
    }
  }
  Mix(x);

  // Counter loops, and conditional branches decrementing CTR.
  for (int i = 0; i < 100; i++)
  {
    u32 count = 0, n = Random() % 100 + 1;
    asm volatile("mtctr %1\n1:\taddi %0,%0,1\n\tbdnz 1b" : "+r"(count) : "r"(n) : "ctr");
    Mix(count);
    count = 0;
    asm volatile("mtctr %1\n1:\taddi %0,%0,1\n\tcmpwi %0,50\n\tbdnzf 2,1b"
                 : "+r"(count)
                 : "r"(n)
                 : "ctr", "cr0");
    Mix(count);
    u32 ctr;
    asm volatile("mtctr %1\n1:\taddi %1,%1,0\n\tbdz 2f\n\tb 1b\n2:\tmfctr %0"
                 : "=r"(ctr)
                 : "r"(n)
                 : "ctr");
    Mix(ctr);
  }

  // A long loop, over many timing slices.
  u32 sum = 0;
  for (u32 i = 0; i < 20000000; i++)
    sum += i ^ (sum >> 3);
  Mix(sum);

  End("branches");
}

// Conditional branches right after the compare or record form that sets their CR field, which
// the JIT tests in the host's comparison, with XER[SO] set or clear: each condition, taken or
// not, sets a bit.
#define COMPARE_BRANCH_1(insn, branch, bit) \
  insn "\n\t" branch ",1f\n\tori %0,%0," #bit "\n1:\n\t"
#define COMPARE_BRANCH(name, field, insn)                                                          \
  do                                                                                               \
  {                                                                                                \
    u32 bits, tmp;                                                                                 \
    asm volatile("mtxer %4\n\tli %0,0\n\t" COMPARE_BRANCH_1(insn, "blt " field, 1)                \
                     COMPARE_BRANCH_1(insn, "bge " field, 2) COMPARE_BRANCH_1(insn, "bgt " field, 4) \
                         COMPARE_BRANCH_1(insn, "ble " field, 8)                                   \
                             COMPARE_BRANCH_1(insn, "beq " field, 16)                              \
                                 COMPARE_BRANCH_1(insn, "bne " field, 32)                          \
                                     COMPARE_BRANCH_1(insn, "bso " field, 64)                      \
                                         COMPARE_BRANCH_1(insn, "bns " field, 128)                 \
                 : "=&r"(bits), "=&r"(tmp)                                                         \
                 : "r"(a), "r"(b), "r"(xer)                                                        \
                 : "cr0", "cr3", "xer");                                                           \
    Record(name, bits, a, b ^ xer);                                                                \
  } while (0)

static void TestCompareAndBranch(void)
{
  Begin();
  for (int i = 0; i < 2000; i++)
  {
    BeginIteration();
    const u32 a = Value();
    const u32 b = (Random() & 3) == 0 ? a : Value();
    const u32 xer = (Random() & 1) << 31;
    COMPARE_BRANCH("cmpw", "cr3", "cmpw cr3,%2,%3");
    COMPARE_BRANCH("cmplw", "cr3", "cmplw cr3,%2,%3");
    COMPARE_BRANCH("cmpwi", "cr3", "cmpwi cr3,%2,-1");
    COMPARE_BRANCH("cmpwi max", "cr3", "cmpwi cr3,%2,0x7FFF");
    COMPARE_BRANCH("cmpwi min", "cr3", "cmpwi cr3,%2,-0x8000");
    COMPARE_BRANCH("cmplwi", "cr3", "cmplwi cr3,%2,0x8000");
    COMPARE_BRANCH("cmplwi max", "cr3", "cmplwi cr3,%2,0xFFFF");
    COMPARE_BRANCH("cmplwi 0", "cr3", "cmplwi cr3,%2,0");
    COMPARE_BRANCH("add.", "cr0", "add. %1,%2,%3");
    COMPARE_BRANCH("and.", "cr0", "and. %1,%2,%3");
    COMPARE_BRANCH("rlwinm.", "cr0", "rlwinm. %1,%2,1,0,31");
  }
  End("compare-branch");
}

// More guest registers than the JIT has host registers for, in one block, with branches out of
// it and within it, so that registers are evicted and flushed on conditional exits.
// The last word is the loop count.
static u32 pressure[29];

// A CR field that the JIT has evicted from its register cache, tested by a branch that also
// decrements CTR: the field has to be loaded on both of the CTR test's paths, and with CTR 1 the
// branch falls through to code that reads it again. Writing 16 GPRs after the compare evicts it.
static void TestCREviction(void)
{
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    const u32 a = Random() % 4, b = Random() % 4, ctr = 1 + Random() % 2;
    u32 cr, fell_through;
    static u32 regs[16];
    asm volatile("cmpw 2,%2,%3\n\t"
                 "addi 14,14,1\n\t"
                 "addi 15,15,1\n\t"
                 "addi 16,16,1\n\t"
                 "addi 17,17,1\n\t"
                 "addi 18,18,1\n\t"
                 "addi 19,19,1\n\t"
                 "addi 20,20,1\n\t"
                 "addi 21,21,1\n\t"
                 "addi 22,22,1\n\t"
                 "addi 23,23,1\n\t"
                 "addi 24,24,1\n\t"
                 "addi 25,25,1\n\t"
                 "addi 26,26,1\n\t"
                 "addi 27,27,1\n\t"
                 "addi 28,28,1\n\t"
                 "addi 29,29,1\n\t"
                 "mtctr %4\n\t"
                 "li %1,0\n\t"
                 "bdnzt 10,1f\n\t"
                 "li %1,1\n"
                 "1:\tmfcr %0\n\t"
                 "stw 14,0(%5)\n\tstw 21,28(%5)\n\tstw 29,60(%5)"
                 : "=&r"(cr), "=&r"(fell_through)
                 : "r"(a), "r"(b), "r"(ctr), "b"(regs)
                 : "r14", "r15", "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26", "r27", "r28", "r29", "cr2", "ctr", "memory");
    Record("bdnzt-evicted", cr, fell_through, regs[0] ^ regs[7] ^ regs[15]);
  }
  End("cr-cache-eviction");
}

// Calls and returns, which a JIT may predict with a stack of its own: returns to the address after
// the call or elsewhere, bl to read the PC, which never returns, calls through CTR and LR, and
// recursion, also across many timing slices (TestDeepRecursion).
static u32 __attribute__((noipa)) Leaf(u32 x)
{
  return x * 3 + 1;
}
static u32 (*volatile leaf)(u32) = Leaf;

static u32 __attribute__((noipa)) Recurse(u32 n);
static u32 (*volatile recurse)(u32) = Recurse;
static u32 __attribute__((noipa)) Recurse(u32 n)
{
  // Through a pointer, so that the compiler can't make a loop of it.
  return n == 0 ? 1 : recurse(n - 1) * 3 + n;
}

#define CALL_CLOBBERS                                                                              \
  "r0", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "ctr", "lr", "cr0", "cr1", "cr5", \
      "cr6", "cr7", "memory"

static void TestCallsAndReturns(void)
{
  Begin();
  for (int i = 0; i < 200; i++)
  {
    BeginIteration();
    u32 a, b;
    // bl to read the PC.
    asm volatile("bl 1f\n1:\tmflr %0\n\tbl 2f\n2:\tmflr %1" : "=&r"(a), "=&r"(b) : : "lr");
    Record("bl pc", b - a, 0, 0);
    // A return past the instruction after the call.
    asm volatile("bl 1f\n\tli %0,1\n\tb 2f\n1:\tmflr 12\n\taddi 12,12,28\n\tmtlr 12\n\tli %0,2\n\tblr\n2:"
                 : "=&r"(a)
                 : : "r12", "lr");
    Record("blr elsewhere", a, 0, 0);
    // Calls through CTR and LR, and returns.
    const u32 x = Random();
    a = leaf(x);
    register u32 r3 asm("r3") = x;
    asm volatile("mtlr %1\n\tblrl" : "+r"(r3) : "r"(Leaf) : CALL_CLOBBERS);
    Record("bctrl blrl", a, r3, 0);
    // Recursion.
    Record("recursion", recurse(Random() % 64), 0, 0);
  }
  End("calls-returns");
}

// Recursion across many timing slices, at whose ends a JIT may drop its stack of returns: the
// returns after that don't find theirs.
static void TestDeepRecursion(void)
{
  Begin();
  BeginIteration();
  Record("deep recursion", recurse(5000), 0, 0);
  Record("recursion after", recurse(50), 0, 0);
  End("deep-recursion");
}

static void TestRegisterPressure(void)
{
  Begin();
  for (int round = 0; round < 200; round++)
  {
    for (int i = 0; i < 28; i++)
      pressure[i] = Value();
    pressure[28] = Random() % 64 + 1;
    register u32* p asm("r3") = pressure;
    asm volatile(
        "lwz 0,112(3)\n\t"
        "mtctr 0\n\t"
        "lwz 4,0(3)\n\t"
        "lwz 5,4(3)\n\t"
        "lwz 6,8(3)\n\t"
        "lwz 7,12(3)\n\t"
        "lwz 8,16(3)\n\t"
        "lwz 9,20(3)\n\t"
        "lwz 10,24(3)\n\t"
        "lwz 11,28(3)\n\t"
        "lwz 12,32(3)\n\t"
        "lwz 14,36(3)\n\t"
        "lwz 15,40(3)\n\t"
        "lwz 16,44(3)\n\t"
        "lwz 17,48(3)\n\t"
        "lwz 18,52(3)\n\t"
        "lwz 19,56(3)\n\t"
        "lwz 20,60(3)\n\t"
        "lwz 21,64(3)\n\t"
        "lwz 22,68(3)\n\t"
        "lwz 23,72(3)\n\t"
        "lwz 24,76(3)\n\t"
        "lwz 25,80(3)\n\t"
        "lwz 26,84(3)\n\t"
        "lwz 27,88(3)\n\t"
        "lwz 28,92(3)\n\t"
        "lwz 29,96(3)\n\t"
        "lwz 30,100(3)\n\t"
        "lwz 31,104(3)\n\t"
        "lwz 0,108(3)\n\t"
        "1:\n\t"
        "add 4,4,5\n\t"
        "rotlwi 4,4,1\n\t"
        "add 5,5,6\n\t"
        "add 6,6,7\n\t"
        "add 7,7,8\n\t"
        "cmpwi 7,0\n\t"
        "blt 2f\n\t"
        "xori 8,8,0x5a5a\n\t"
        "2:\n\t"
        "add 8,8,9\n\t"
        "rotlwi 8,8,29\n\t"
        "add 9,9,10\n\t"
        "add 10,10,11\n\t"
        "add 11,11,12\n\t"
        "add 12,12,14\n\t"
        "rotlwi 12,12,26\n\t"
        "add 14,14,15\n\t"
        "add 15,15,16\n\t"
        "add 16,16,17\n\t"
        "add 17,17,18\n\t"
        "rotlwi 17,17,23\n\t"
        "cmpwi 17,0\n\t"
        "blt 2f\n\t"
        "xori 18,18,0x5a5a\n\t"
        "2:\n\t"
        "add 18,18,19\n\t"
        "add 19,19,20\n\t"
        "add 20,20,21\n\t"
        "add 21,21,22\n\t"
        "rotlwi 21,21,20\n\t"
        "add 22,22,23\n\t"
        "add 23,23,24\n\t"
        "add 24,24,25\n\t"
        "add 25,25,26\n\t"
        "rotlwi 25,25,17\n\t"
        "add 26,26,27\n\t"
        "cmpwi 26,0\n\t"
        "blt 2f\n\t"
        "xori 27,27,0x5a5a\n\t"
        "2:\n\t"
        "add 27,27,28\n\t"
        "add 28,28,29\n\t"
        "add 29,29,30\n\t"
        "rotlwi 29,29,14\n\t"
        "add 30,30,31\n\t"
        "add 31,31,0\n\t"
        "add 0,0,4\n\t"
        "bdnz 1b\n\t"
        "stw 4,0(3)\n\t"
        "stw 5,4(3)\n\t"
        "stw 6,8(3)\n\t"
        "stw 7,12(3)\n\t"
        "stw 8,16(3)\n\t"
        "stw 9,20(3)\n\t"
        "stw 10,24(3)\n\t"
        "stw 11,28(3)\n\t"
        "stw 12,32(3)\n\t"
        "stw 14,36(3)\n\t"
        "stw 15,40(3)\n\t"
        "stw 16,44(3)\n\t"
        "stw 17,48(3)\n\t"
        "stw 18,52(3)\n\t"
        "stw 19,56(3)\n\t"
        "stw 20,60(3)\n\t"
        "stw 21,64(3)\n\t"
        "stw 22,68(3)\n\t"
        "stw 23,72(3)\n\t"
        "stw 24,76(3)\n\t"
        "stw 25,80(3)\n\t"
        "stw 26,84(3)\n\t"
        "stw 27,88(3)\n\t"
        "stw 28,92(3)\n\t"
        "stw 29,96(3)\n\t"
        "stw 30,100(3)\n\t"
        "stw 31,104(3)\n\t"
        "stw 0,108(3)"
        : "+r"(p)
        :
        : "r0", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r14", "r15", "r16",
          "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26", "r27", "r28",
          "r29", "r30", "r31", "ctr", "cr0", "memory");
    for (int i = 0; i < 28; i++)
      Mix(pressure[i]);
  }
  End("registers");
}

// The JITs don't update the FPSCR's status bits, as the interpreter does, so cputest-jit.elf only
// compares the bits that they keep: the control bits, and FPRF if it's enabled.
#ifndef FPSCR_MASK
#define FPSCR_MASK 0xFFFFFFFFu
#endif

static u64 Bits(double d)
{
  union
  {
    double d;
    u64 u;
  } x = {d};
  return x.u;
}

static double Double(u64 u)
{
  union
  {
    u64 u;
    double d;
  } x = {u};
  return x.d;
}

static double FloatValue(void)
{
  static const u64 edge[] = {
      0x0000000000000000, 0x8000000000000000, 0x3FF0000000000000, 0xBFF0000000000000,
      0x7FF0000000000000, 0xFFF0000000000000, 0x7FF8000000000000, 0x7FF4000000000000,  // NaNs
      0x0000000000000001, 0x000FFFFFFFFFFFFF, 0x8008000000000000,                      // Denormal
      0x7FEFFFFFFFFFFFFF, 0x0010000000000000, 0x3810000000000000, 0x36A0000000000000,  // Single limits
      0x47EFFFFFE0000000, 0x41E0000000000000, 0xC1E0000000200000, 0x3FE0000000000000,
  };
  const u32 r = Random();
  if (r & 1)
    return Double(edge[(r >> 1) % (sizeof(edge) / sizeof(edge[0]))]);
  // Random sign and mantissa, with exponents around the single and double ranges.
  const u64 exponent = (r >> 1) % 4 == 0 ? (Random() % 2048) : 1023 - 140 + (Random() % 280);
  return Double(((u64)(Random() & 0x80000000) << 32) | (exponent << 52) |
                (((u64)Random() << 32 | Random()) & 0x000FFFFFFFFFFFFF));
}

// Sets the FPSCR to fpscr, runs the instruction, and captures the result, FPSCR and CR1.
#define FOP(insn, ...)                                                                             \
  do                                                                                               \
  {                                                                                                \
    double r_, s_;                                                                                 \
    u32 c_;                                                                                        \
    asm volatile("mtfsf 0xff,%3\n\t" insn "\n\tmffs %1\n\tmfcr %2"                                 \
                 : "=&f"(r_), "=&f"(s_), "=&r"(c_)                                                 \
                 : "f"(fpscr), "f"(a), "f"(b), "f"(c)                                              \
                 : "cr1", "cr2");                                                                  \
    Record64(insn, Bits(r_), Bits(s_) & FPSCR_MASK, (c_ >> 20) & 0xFFF);                                        \
  } while (0)

static void TestFloatingPoint(u32 non_ieee)
{
  Begin();
  for (u32 rounding = 0; rounding < 4; rounding++)
  {
    const double fpscr = Double(rounding | non_ieee);
    for (int i = 0; i < ITERATIONS; i++)
    {
      BeginIteration();
      const double a = FloatValue(), b = FloatValue(), c = FloatValue();
      FOP("fadd %0,%4,%5");
      FOP("fadd. %0,%4,%5");
      FOP("fsub %0,%4,%5");
      FOP("fmul %0,%4,%6");
      FOP("fmul. %0,%4,%6");
      FOP("fdiv %0,%4,%5");
      FOP("fdiv. %0,%4,%5");
      FOP("fadds %0,%4,%5");
      FOP("fsubs %0,%4,%5");
      FOP("fmuls %0,%4,%6");
      FOP("fdivs %0,%4,%5");
      FOP("fmadd %0,%4,%6,%5");
      FOP("fmsub %0,%4,%6,%5");
      FOP("fnmadd %0,%4,%6,%5");
      FOP("fnmsub. %0,%4,%6,%5");
      FOP("fmadds %0,%4,%6,%5");
      FOP("fmsubs %0,%4,%6,%5");
      FOP("fnmadds %0,%4,%6,%5");
      FOP("fnmsubs %0,%4,%6,%5");
      FOP("frsp %0,%4");
      FOP("frsp. %0,%4");
      FOP("fctiw %0,%4");
      FOP("fctiwz %0,%4");
      FOP("fres %0,%4");
      FOP("frsqrte %0,%4");
      FOP("fsel %0,%4,%6,%5");
      FOP("fabs %0,%4");
      FOP("fnabs %0,%4");
      FOP("fneg %0,%4");
      FOP("fmr %0,%4");
      FOP("fcmpu 2,%4,%5\n\tfmr %0,%4");
      FOP("fcmpo 2,%4,%5\n\tfmr %0,%4");
      FOP("mtfsb1 3\n\tmtfsb0 22\n\tmtfsfi 6,9\n\tfmr %0,%4");
    }
  }
  End(non_ieee ? "floating-point-ni" : "floating-point");
}

// Single precision fused multiply-adds on inputs whose results are ties when rounded from double
// to single precision, which the interpreter corrects (see NI_madd_msub), and on NaNs for frC whose
// payloads change when frC is rounded to 25 bits.
static float FloatBits(u32 u);

static void TestFusedMultiplyAddCases(void)
{
  static const u32 ties[][3] = {
      // a, b, c
      {0x42480000, 0x1b1c72a0, 0xbc88cc38},  // From Mario Strikers Charged
      {0xc1b4fd47, 0x29ff8325, 0xbf019c77},
      {0xc2118627, 0xb47a1e02, 0xc13b95b2},
      {0xbc790257, 0xaabfede6, 0xc3471e58},
      {0xbd881ae9, 0x2a9fca37, 0xc289d2bd},
      {0x4280dc53, 0xb3d01405, 0x416a9cb9},
  };
  static const u64 nans[] = {
      0x7FF800000FFFFFFF, 0x7FF4000008000000, 0xFFF000000FFFFFFF, 0x7FF0000000000001,
  };
  Begin();
  const double fpscr = Double(0);
  for (u32 i = 0; i < sizeof(ties) / sizeof(ties[0]); i++)
  {
    BeginIteration();
    const double a = FloatBits(ties[i][0]), b = FloatBits(ties[i][1]), c = FloatBits(ties[i][2]);
    FOP("fmadds %0,%4,%6,%5");
    FOP("fmsubs %0,%4,%6,%5");
    FOP("fnmadds %0,%4,%6,%5");
    FOP("fnmsubs %0,%4,%6,%5");
  }
  for (u32 i = 0; i < 2 * sizeof(nans) / sizeof(nans[0]); i++)
  {
    BeginIteration();
    const double a = (i & 1) ? 1.0 : 0.0, b = 2.0, c = Double(nans[i / 2]);
    FOP("fmadds %0,%4,%6,%5");
    FOP("fnmsubs %0,%4,%6,%5");
    FOP("fmuls %0,%4,%6");
  }
  End("fma-cases");
}

// Floating-point loads and stores, including stores of doubles too small for normal singles,
// which the store converts specially (see ConvertToSingle).
static void TestFloatLoadsAndStores(void)
{
  static u64 buffer[8] __attribute__((aligned(32)));
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    for (int j = 0; j < 8; j++)
      buffer[j] = ((u64)Random() << 32) | Random();
    // Exponents from 860 to 910 are around the smallest singles.
    double value = FloatValue();
    if (Random() & 1)
      value = Double((Bits(value) & 0x800FFFFFFFFFFFFFULL) | ((u64)(860 + Random() % 50) << 52));
    const u32 index = 8 * (Random() % 4);
    u8* const base = (u8*)buffer;
    double s0, s1, d0, d1;
    u8* p = base;
    asm volatile("lfs %0,4(%5)\n\tlfsx %1,%5,%6\n\tlfd %2,8(%5)\n\tlfdux %3,%4,%6"
                 : "=&f"(s0), "=&f"(s1), "=&f"(d0), "=&f"(d1), "+b"(p)
                 : "b"(base), "r"(index)
                 : "memory");
    Record64("lfs", Bits(s0), Bits(s1), 0);
    Record64("lfd", Bits(d0), Bits(d1), (u32)(p - base));
    p = base + 32;
    asm volatile("stfs %1,0(%2)\n\tstfsu %1,4(%0)\n\tstfd %1,8(%2)\n\tstfdx %1,%2,%3\n\t"
                 "stfiwx %1,%0,%3"
                 : "+b"(p)
                 : "f"(value), "b"(base + 16), "r"(index)
                 : "memory");
    Record("stfs", (u32)(p - base), 0, 0);
    for (int j = 0; j < 8; j++)
      Mix64(buffer[j]);
  }
  End("float-loads-stores");
}

// A paired-single instruction on fr1, fr2 and fr3, loaded from inputs with psq_l as singles, which
// the JIT knows they are: it computes both halves at once then, unlike with the doubles of PSOPD.
#define PSOP(insn)                                                                                 \
  do                                                                                               \
  {                                                                                                \
    double r_, s_;                                                                                 \
    u32 c_;                                                                                        \
    double out[2];                                                                                 \
    asm volatile("mtfsf 0xff,%3\n\tpsq_l 1,0(%4),0,0\n\tpsq_l 2,8(%4),0,0\n\tpsq_l 3,16(%4),0,0\n\t" \
                 insn "\n\tmffs %1\n\tmfcr %2\n\tstfd %0,0(%5)\n\tps_merge11 %0,%0,%0\n\tstfd %0,8(%5)" \
                 : "=&f"(r_), "=&f"(s_), "=&r"(c_)                                                 \
                 : "f"(fpscr), "b"(inputs), "b"(out)                                               \
                 : "fr1", "fr2", "fr3", "cr1", "cr2", "memory");                                   \
    Record64(insn, Bits(out[0]), Bits(out[1]), (c_ >> 20) & 0xFFF);                                \
    Mix64(Bits(s_) & FPSCR_MASK);                                                                       \
  } while (0)

static float FloatBits(u32 u)
{
  union
  {
    u32 u;
    float f;
  } x = {u};
  return x.f;
}

static float SingleValue(void)
{
  static const u32 edge[] = {0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000,
                             0xFF800000, 0x7FC00000, 0x7FA00000, 0x00000001, 0x007FFFFF,
                             0x7F7FFFFF, 0x00800000, 0x3F000000, 0x4B000000};
  const u32 r = Random();
  if (r & 1)
    return FloatBits(edge[(r >> 1) % (sizeof(edge) / sizeof(edge[0]))]);
  return FloatBits((Random() & 0x80000000) | ((80 + Random() % 100) << 23) |
                   (Random() & 0x007FFFFF));
}

static void TestPairedSingles(void)
{
  Begin();
  float inputs[6] __attribute__((aligned(8)));
  // Each rounding mode, without and with FPSCR[NI].
  for (u32 rounding = 0; rounding < 8; rounding++)
  {
    const double fpscr = Double(rounding);
    for (int i = 0; i < ITERATIONS; i++)
    {
      BeginIteration();
      for (int j = 0; j < 6; j++)
        inputs[j] = SingleValue();
      PSOP("ps_add %0,1,2");
      PSOP("ps_sub %0,1,2");
      PSOP("ps_mul %0,1,3");
      PSOP("ps_div %0,1,2");
      PSOP("ps_madd %0,1,3,2");
      PSOP("ps_msub %0,1,3,2");
      PSOP("ps_nmadd %0,1,3,2");
      PSOP("ps_nmsub %0,1,3,2");
      PSOP("ps_sum0 %0,1,3,2");
      PSOP("ps_sum1 %0,1,3,2");
      PSOP("ps_muls0 %0,1,3");
      PSOP("ps_muls1 %0,1,3");
      PSOP("ps_madds0 %0,1,3,2");
      PSOP("ps_madds1 %0,1,3,2");
      PSOP("ps_merge00 %0,1,2");
      PSOP("ps_merge01 %0,1,2");
      PSOP("ps_merge10 %0,1,2");
      PSOP("ps_merge11 %0,1,2");
      PSOP("ps_res %0,1");
      PSOP("ps_rsqrte %0,1");
      PSOP("ps_sel %0,1,3,2");
      PSOP("ps_neg %0,1");
      PSOP("ps_abs %0,1");
      PSOP("ps_nabs %0,1");
      PSOP("ps_mr %0,1");
      PSOP("ps_cmpu0 2,1,2\n\tps_mr %0,1");
      PSOP("ps_cmpo1 2,1,2\n\tps_mr %0,1");
      PSOP("fadd %0,1,2");
      PSOP("fmuls %0,1,3");
    }
  }
  End("paired-singles");
}

// Paired-single fused multiply-adds and multiplies on the cases of TestFusedMultiplyAddCases, in
// both halves, with the halves made from doubles, so that frC can be a NaN that changes when it's
// rounded to 25 bits.
#define PSOPD(insn)                                                                                \
  do                                                                                               \
  {                                                                                                \
    double r_, s_;                                                                                 \
    u32 c_;                                                                                        \
    double out[2];                                                                                 \
    asm volatile("mtfsf 0xff,%3\n\tlfd 1,0(%4)\n\tlfd 4,8(%4)\n\tps_merge00 1,1,4\n\t"             \
                 "lfd 2,16(%4)\n\tlfd 4,24(%4)\n\tps_merge00 2,2,4\n\t"                            \
                 "lfd 3,32(%4)\n\tlfd 4,40(%4)\n\tps_merge00 3,3,4\n\t" insn                       \
                 "\n\tmffs %1\n\tmfcr %2\n\tstfd %0,0(%5)\n\tps_merge11 %0,%0,%0\n\tstfd %0,8(%5)" \
                 : "=&f"(r_), "=&f"(s_), "=&r"(c_)                                                 \
                 : "f"(fpscr), "b"(inputs), "b"(out)                                               \
                 : "fr1", "fr2", "fr3", "fr4", "cr1", "cr2", "memory");                            \
    Record64(insn, Bits(out[0]), Bits(out[1]), (c_ >> 20) & 0xFFF);                                \
    Mix64(Bits(s_) & FPSCR_MASK);                                                                  \
  } while (0)

// FPSCR instructions, and the record forms of floating-point and paired-single instructions. The
// JITs don't update FPSCR's status bits, so FPSCR is cleared before each record form, whose CR1
// comes from them, and mffs is masked.
// Loads and stores of every kind through the uncached mirror of RAM, which the BATs map but
// fastmem doesn't, so that they take the JIT's slow paths, also without the MMU. Each store
// writes its own bytes, which are then read through the cached mirror.
static u8 slow_memory[512] __attribute__((aligned(64)));

static void TestSlowPaths(void)
{
  Begin();
  u8* const uncached = (u8*)((u32)slow_memory | 0x40000000);
  for (int i = 0; i < ITERATIONS / 4; i++)
  {
    BeginIteration();
    for (u32 j = 0; j < sizeof(slow_memory); j += 4)
      *(u32*)&slow_memory[j] = Random();
    const u32 value = Value();
    const double fvalue = FloatValue();
    const u32 index = 4 * (Random() % 8);
    u32 r;
    u8* p;

#define SLOW_LOAD(insn, offset)                                                                    \
  do                                                                                               \
  {                                                                                                \
    asm volatile(insn " %0," #offset "(%1)" : "=r"(r) : "b"(uncached) : "memory");                 \
    Record(insn, r, 0, 0);                                                                         \
  } while (0)
#define SLOW_LOADX(insn)                                                                           \
  do                                                                                               \
  {                                                                                                \
    asm volatile(insn " %0,%1,%2" : "=r"(r) : "b"(uncached + 64), "r"(index) : "memory");          \
    Record(insn, r, 0, 0);                                                                         \
  } while (0)
#define SLOW_LOADU(insn, offset)                                                                   \
  do                                                                                               \
  {                                                                                                \
    p = uncached;                                                                                  \
    asm volatile(insn " %0," #offset "(%1)" : "=r"(r), "+b"(p) : : "memory");                      \
    Record(insn, r, (u32)(p - uncached), 0);                                                       \
  } while (0)
    SLOW_LOAD("lbz", 3);
    SLOW_LOAD("lhz", 6);
    SLOW_LOAD("lha", 10);
    SLOW_LOAD("lwz", 16);
    SLOW_LOAD("lwz", 21);
    SLOW_LOADX("lbzx");
    SLOW_LOADX("lhzx");
    SLOW_LOADX("lhax");
    SLOW_LOADX("lwzx");
    SLOW_LOADX("lhbrx");
    SLOW_LOADX("lwbrx");
    SLOW_LOADU("lbzu", 5);
    SLOW_LOADU("lhau", 8);
    SLOW_LOADU("lwzu", 12);
    {
      double f;
      asm volatile("lfs %0,24(%1)" : "=f"(f) : "b"(uncached) : "memory");
      Record64("lfs", Bits(f), 0, 0);
      asm volatile("lfd %0,32(%1)" : "=f"(f) : "b"(uncached) : "memory");
      Record64("lfd", Bits(f), 0, 0);
      asm volatile("lfdx %0,%1,%2" : "=f"(f) : "b"(uncached + 64), "r"(index) : "memory");
      Record64("lfdx", Bits(f), 0, 0);
      p = uncached;
      asm volatile("lfsu %0,40(%1)" : "=f"(f), "+b"(p) : : "memory");
      Record64("lfsu", Bits(f), (u32)(p - uncached), 0);
      // Paired singles, with GQR0 (floats), and with a type that the block sets in GQR1.
      double f1;
      asm volatile("psq_l %0,48(%2),0,0\n\tps_merge11 %1,%0,%0" : "=f"(f), "=f"(f1)
                   : "b"(uncached) : "memory");
      Record64("psq_l", Bits(f), Bits(f1), 0);
      asm volatile("psq_l %0,48(%2),1,0\n\tps_merge11 %1,%0,%0" : "=f"(f), "=f"(f1)
                   : "b"(uncached) : "memory");
      Record64("psq_l single", Bits(f), Bits(f1), 0);
      static const u32 types[] = {0, 4, 5, 6, 7};
      const u32 gqr = (types[Random() % 5] << 16) | (Random() % 64) << 24 | types[Random() % 5] |
                      (Random() % 64) << 8;
      asm volatile("mtspr %3,%4\n\tpsq_l %0,56(%2),0,1\n\tps_merge11 %1,%0,%0" : "=&f"(f), "=f"(f1)
                   : "b"(uncached), "i"(913), "r"(gqr) : "memory");
      Record64("psq_l gqr", Bits(f), Bits(f1), 0);
      double ps = fvalue;
      asm volatile("ps_merge00 %0,%0,%0" : "+f"(ps));
      // Stores, each to its own bytes, from offset 128.
      asm volatile("stb %0,128(%1)\n\tsth %0,136(%1)\n\tstw %0,144(%1)" : : "r"(value), "b"(uncached)
                   : "memory");
      asm volatile("sthbrx %0,0,%1\n\tstwbrx %0,0,%2" : : "r"(value), "b"(uncached + 152),
                   "b"(uncached + 160) : "memory");
      asm volatile("stfs %0,168(%1)\n\tstfd %0,176(%1)\n\tstfiwx %0,0,%2" : : "f"(fvalue),
                   "b"(uncached), "b"(uncached + 184) : "memory");
      asm volatile("psq_st %0,192(%1),0,0\n\tpsq_st %0,200(%1),1,0" : : "f"(ps), "b"(uncached)
                   : "memory");
      asm volatile("mtspr %2,%3\n\tpsq_st %0,208(%1),0,1" : : "f"(ps), "b"(uncached), "i"(913),
                   "r"(gqr) : "memory");
      p = uncached + 216;
      asm volatile("stwu %1,8(%0)\n\tstbu %1,8(%0)\n\tsthu %1,8(%0)" : "+b"(p) : "r"(value) : "memory");
      Record("stores update", (u32)(p - uncached), 0, 0);
      // Unaligned and of less than a word, which uncached memory writes differently.
      asm volatile("sth %0,251(%1)\n\tstw %0,258(%1)\n\tstb %0,269(%1)" : : "r"(value), "b"(uncached)
                   : "memory");
      // Multiple words, and a line zeroed.
      asm volatile("mr 30,%0\n\tmr 31,%1\n\tstmw 30,280(%2)\n\tlmw 30,280(%2)\n\tmr %0,30"
                   : "+r"(r) : "r"(value), "b"(uncached) : "r30", "r31", "memory");
      Record("stmw lmw", r, 0, 0);
      asm volatile("dcbz 0,%0" : : "b"(uncached + 320) : "memory");
    }
    for (u32 j = 0; j < sizeof(slow_memory); j += 4)
      Mix(*(u32*)&slow_memory[j]);
  }
  End("slow-paths");
}

static double FromBits(u64 bits)
{
  union
  {
    u64 i;
    double d;
  } u = {bits};
  return u.d;
}

// The FPSCR instructions on random FPSCR values: the exception summaries VX and FEX, FX, the bits
// that mcrfs clears, and the rounding mode and NI, which a compiled fadd after them uses. The fadd
// comes after FPSCR is read, as the JIT doesn't set the bits that arithmetic sets, and runs with
// the exception enable bits clear, which the JIT ignores. Then mcrxr, and mffs, which keeps ps1.
// FPSCR, which a JIT may keep in a host register, changed and then read by the interpreter in the
// same block: by an instruction that the JIT interprets (a record form), and by one that it calls
// the interpreter for from far code (fres of zero). mtfsb1 sets ZE, which neither changes.
static void TestFPSCRAndInterpreter(void)
{
  Begin();
  for (int i = 0; i < 200; i++)
  {
    BeginIteration();
    const double fpscr = Double(i % 4);
    const double a = FloatValue(), b = FloatValue(), zero = (i & 1) ? -0.0 : 0.0;
    double d, f;
    u32 cr;
    asm volatile("mtfsf 0xff,%3\n\tmtfsb1 27\n\tfadd. %0,%4,%5\n\tmfcr %2\n\tmffs %1"
                 : "=&f"(d), "=&f"(f), "=&r"(cr)
                 : "f"(fpscr), "f"(a), "f"(b)
                 : "cr1");
    Record64("mtfsb1 fadd.", Bits(d), Bits(f) & FPSCR_MASK, (cr >> 24) & 0xF);
    d = a;
    asm volatile("mtfsf 0xff,%2\n\tmtfsb1 27\n\tfres %0,%3\n\tmffs %1"
                 : "+f"(d), "=&f"(f)
                 : "f"(fpscr), "f"(zero));
    Record64("mtfsb1 fres", Bits(d), Bits(f) & FPSCR_MASK, 0);
  }
  asm volatile("mtfsf 0xff,%0" : : "f"(Double(0)));
  End("fpscr-interpreter");
}

static void TestFPSCRInstructions(void)
{
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    // Without the enable bits, so that nothing would raise an exception.
    const double start = FromBits(Random() & ~0xF8u);
    const double value = FromBits(((u64)Random() << 32) | (Random() & ~0xF8u));
    const double a = FloatValue(), b = FloatValue();
    double d, fpscr;
    u32 cr;
#define FPSCR_OP(insn)                                                                             \
  asm volatile("mtfsf 0xff,%3\n\t" insn "\n\tmffs %1\n\tmtfsfi 6,0\n\tmtfsb0 28\n\tfadd %0,%5,%6\n\t" \
               "mfcr %2\n\tmtfsf 0xff,%7" \
               : "=&f"(d), "=&f"(fpscr), "=&r"(cr)                                                \
               : "f"(start), "f"(value), "f"(a), "f"(b), "f"(0.0)                                 \
               : "cr2")
    switch (Random() % 8)
    {
    case 0:
      FPSCR_OP("mtfsf 0xff,%4");
      break;
    case 1:
      FPSCR_OP("mtfsf 0x01,%4");
      break;
    case 2:
      FPSCR_OP("mtfsf 0x80,%4");
      break;
    case 3:
      FPSCR_OP("mtfsf 0x81,%4");
      break;
    case 4:
      FPSCR_OP("mtfsf 0x3c,%4");
      break;
    case 5:
      FPSCR_OP("mtfsf 0xfe,%4");
      break;
    case 6:
      FPSCR_OP("mtfsf 0x7f,%4");
      break;
    case 7:
      FPSCR_OP("mtfsf 0x55,%4");
      break;
    }
    Record64("mtfsf", Bits(d), Bits(fpscr), cr & 0x00F00000);
    switch (Random() % 64)
    {
    case 0:
      FPSCR_OP("mtfsb0 0");
      break;
    case 1:
      FPSCR_OP("mtfsb1 0");
      break;
    case 2:
      FPSCR_OP("mtfsb0 1");
      break;
    case 3:
      FPSCR_OP("mtfsb1 1");
      break;
    case 4:
      FPSCR_OP("mtfsb0 2");
      break;
    case 5:
      FPSCR_OP("mtfsb1 2");
      break;
    case 6:
      FPSCR_OP("mtfsb0 3");
      break;
    case 7:
      FPSCR_OP("mtfsb1 3");
      break;
    case 8:
      FPSCR_OP("mtfsb0 4");
      break;
    case 9:
      FPSCR_OP("mtfsb1 4");
      break;
    case 10:
      FPSCR_OP("mtfsb0 5");
      break;
    case 11:
      FPSCR_OP("mtfsb1 5");
      break;
    case 12:
      FPSCR_OP("mtfsb0 6");
      break;
    case 13:
      FPSCR_OP("mtfsb1 6");
      break;
    case 14:
      FPSCR_OP("mtfsb0 7");
      break;
    case 15:
      FPSCR_OP("mtfsb1 7");
      break;
    case 16:
      FPSCR_OP("mtfsb0 8");
      break;
    case 17:
      FPSCR_OP("mtfsb1 8");
      break;
    case 18:
      FPSCR_OP("mtfsb0 9");
      break;
    case 19:
      FPSCR_OP("mtfsb1 9");
      break;
    case 20:
      FPSCR_OP("mtfsb0 10");
      break;
    case 21:
      FPSCR_OP("mtfsb1 10");
      break;
    case 22:
      FPSCR_OP("mtfsb0 11");
      break;
    case 23:
      FPSCR_OP("mtfsb1 11");
      break;
    case 24:
      FPSCR_OP("mtfsb0 12");
      break;
    case 25:
      FPSCR_OP("mtfsb1 12");
      break;
    case 26:
      FPSCR_OP("mtfsb0 13");
      break;
    case 27:
      FPSCR_OP("mtfsb1 13");
      break;
    case 28:
      FPSCR_OP("mtfsb0 14");
      break;
    case 29:
      FPSCR_OP("mtfsb1 14");
      break;
    case 30:
      FPSCR_OP("mtfsb0 15");
      break;
    case 31:
      FPSCR_OP("mtfsb1 15");
      break;
    case 32:
      FPSCR_OP("mtfsb0 16");
      break;
    case 33:
      FPSCR_OP("mtfsb1 16");
      break;
    case 34:
      FPSCR_OP("mtfsb0 17");
      break;
    case 35:
      FPSCR_OP("mtfsb1 17");
      break;
    case 36:
      FPSCR_OP("mtfsb0 18");
      break;
    case 37:
      FPSCR_OP("mtfsb1 18");
      break;
    case 38:
      FPSCR_OP("mtfsb0 19");
      break;
    case 39:
      FPSCR_OP("mtfsb1 19");
      break;
    case 40:
      FPSCR_OP("mtfsb0 20");
      break;
    case 41:
      FPSCR_OP("mtfsb1 20");
      break;
    case 42:
      FPSCR_OP("mtfsb0 21");
      break;
    case 43:
      FPSCR_OP("mtfsb1 21");
      break;
    case 44:
      FPSCR_OP("mtfsb0 22");
      break;
    case 45:
      FPSCR_OP("mtfsb1 22");
      break;
    case 46:
      FPSCR_OP("mtfsb0 23");
      break;
    case 47:
      FPSCR_OP("mtfsb1 23");
      break;
    case 48:
      FPSCR_OP("mtfsb0 24");
      break;
    case 49:
      FPSCR_OP("mtfsb1 24");
      break;
    case 50:
      FPSCR_OP("mtfsb0 25");
      break;
    case 51:
      FPSCR_OP("mtfsb1 25");
      break;
    case 52:
      FPSCR_OP("mtfsb0 26");
      break;
    case 53:
      FPSCR_OP("mtfsb1 26");
      break;
    case 54:
      FPSCR_OP("mtfsb0 27");
      break;
    case 55:
      FPSCR_OP("mtfsb1 27");
      break;
    case 56:
      FPSCR_OP("mtfsb0 28");
      break;
    case 57:
      FPSCR_OP("mtfsb1 28");
      break;
    case 58:
      FPSCR_OP("mtfsb0 29");
      break;
    case 59:
      FPSCR_OP("mtfsb1 29");
      break;
    case 60:
      FPSCR_OP("mtfsb0 30");
      break;
    case 61:
      FPSCR_OP("mtfsb1 30");
      break;
    case 62:
      FPSCR_OP("mtfsb0 31");
      break;
    case 63:
      FPSCR_OP("mtfsb1 31");
      break;
    }
    Record64("mtfsb", Bits(d), Bits(fpscr), cr & 0x00F00000);
    switch (Random() % 32)
    {
    case 0:
      FPSCR_OP("mtfsfi 0,0");
      break;
    case 1:
      FPSCR_OP("mtfsfi 0,5");
      break;
    case 2:
      FPSCR_OP("mtfsfi 0,10");
      break;
    case 3:
      FPSCR_OP("mtfsfi 0,15");
      break;
    case 4:
      FPSCR_OP("mtfsfi 1,0");
      break;
    case 5:
      FPSCR_OP("mtfsfi 1,5");
      break;
    case 6:
      FPSCR_OP("mtfsfi 1,10");
      break;
    case 7:
      FPSCR_OP("mtfsfi 1,15");
      break;
    case 8:
      FPSCR_OP("mtfsfi 2,0");
      break;
    case 9:
      FPSCR_OP("mtfsfi 2,5");
      break;
    case 10:
      FPSCR_OP("mtfsfi 2,10");
      break;
    case 11:
      FPSCR_OP("mtfsfi 2,15");
      break;
    case 12:
      FPSCR_OP("mtfsfi 3,0");
      break;
    case 13:
      FPSCR_OP("mtfsfi 3,5");
      break;
    case 14:
      FPSCR_OP("mtfsfi 3,10");
      break;
    case 15:
      FPSCR_OP("mtfsfi 3,15");
      break;
    case 16:
      FPSCR_OP("mtfsfi 4,0");
      break;
    case 17:
      FPSCR_OP("mtfsfi 4,5");
      break;
    case 18:
      FPSCR_OP("mtfsfi 4,10");
      break;
    case 19:
      FPSCR_OP("mtfsfi 4,15");
      break;
    case 20:
      FPSCR_OP("mtfsfi 5,0");
      break;
    case 21:
      FPSCR_OP("mtfsfi 5,5");
      break;
    case 22:
      FPSCR_OP("mtfsfi 5,10");
      break;
    case 23:
      FPSCR_OP("mtfsfi 5,15");
      break;
    case 24:
      FPSCR_OP("mtfsfi 6,0");
      break;
    case 25:
      FPSCR_OP("mtfsfi 6,5");
      break;
    case 26:
      FPSCR_OP("mtfsfi 6,10");
      break;
    case 27:
      FPSCR_OP("mtfsfi 6,15");
      break;
    case 28:
      FPSCR_OP("mtfsfi 7,0");
      break;
    case 29:
      FPSCR_OP("mtfsfi 7,5");
      break;
    case 30:
      FPSCR_OP("mtfsfi 7,10");
      break;
    case 31:
      FPSCR_OP("mtfsfi 7,15");
      break;
    }
    Record64("mtfsfi", Bits(d), Bits(fpscr), cr & 0x00F00000);
    switch (Random() % 8)
    {
    case 0:
      FPSCR_OP("mcrfs 2,0");
      break;
    case 1:
      FPSCR_OP("mcrfs 2,1");
      break;
    case 2:
      FPSCR_OP("mcrfs 2,2");
      break;
    case 3:
      FPSCR_OP("mcrfs 2,3");
      break;
    case 4:
      FPSCR_OP("mcrfs 2,4");
      break;
    case 5:
      FPSCR_OP("mcrfs 2,5");
      break;
    case 6:
      FPSCR_OP("mcrfs 2,6");
      break;
    case 7:
      FPSCR_OP("mcrfs 2,7");
      break;
    }
    Record64("mcrfs", Bits(d), Bits(fpscr), cr & 0x00F00000);
#undef FPSCR_OP

    u32 xer;
    asm volatile("mtxer %2\n\tmcrxr 3\n\tmfcr %0\n\tmfxer %1"
                 : "=&r"(cr), "=r"(xer)
                 : "r"(Random() & 0xE000007F)
                 : "cr3", "xer");
    Record("mcrxr", cr & 0x000F0000, xer, 0);

    double ps0, ps1;
    asm volatile("mtfsf 0xff,%3\n\tps_merge00 %0,%2,%2\n\tmffs %0\n\tps_merge11 %1,%0,%0\n\t"
                 "mtfsf 0xff,%4"
                 : "=&f"(ps0), "=&f"(ps1)
                 : "f"(a), "f"(start), "f"(0.0));
    Record64("mffs", Bits(ps0), Bits(ps1), 0);
  }
  End("fpscr");
}

static void TestFPSCRAndRecordForms(void)
{
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    BeginIteration();
    const double a = FloatValue(), b = FloatValue(), c = FloatValue();
    double d;
    u32 cr;
    double s_;

#define RECORD_FORM(insn, operands, ...)                                                           \
  do                                                                                               \
  {                                                                                                \
    asm volatile("mtfsf 0xff,%3\n\t" insn " %0," operands "\n\tmfcr %1\n\tmffs %2"               \
                 : "=&f"(d), "=&r"(cr), "=&f"(s_)                                                 \
                 : "f"(0.0), __VA_ARGS__);                                                         \
    Record64(insn, Bits(d), Bits(s_) & FPSCR_MASK, cr & 0x0F000000);                               \
  } while (0)
    RECORD_FORM("fadd.", "%4,%5", "f"(a), "f"(b));
    RECORD_FORM("fmul.", "%4,%5", "f"(a), "f"(c));
    RECORD_FORM("fmadd.", "%4,%5,%6", "f"(a), "f"(c), "f"(b));
    RECORD_FORM("fnmsubs.", "%4,%5,%6", "f"(a), "f"(c), "f"(b));
    RECORD_FORM("fres.", "%4", "f"(b));
    RECORD_FORM("frsqrte.", "%4", "f"(b));
    RECORD_FORM("ps_add.", "%4,%5", "f"(a), "f"(b));
    RECORD_FORM("ps_madd.", "%4,%5,%6", "f"(a), "f"(c), "f"(b));
    RECORD_FORM("ps_res.", "%4", "f"(b));
    RECORD_FORM("ps_rsqrte.", "%4", "f"(b));
#undef RECORD_FORM

    // mtfsfi and mtfsb0/mtfsb1 on the rounding mode and NI, read back by mcrfs, and used by a
    // compiled instruction after them.
    const u32 field7 = Random() % 8;
    asm volatile("mtfsf 0xff,%5\n\tmtfsfi 7,%6\n\tfadd %0,%3,%4\n\tmcrfs 2,7\n\tmfcr %1\n\t"
                 "mtfsb1 29\n\tmtfsb0 30\n\tfmul %2,%3,%4\n\tmtfsf 0xff,%5"
                 : "=&f"(d), "=&r"(cr), "=&f"(s_)
                 : "f"(a), "f"(b), "f"(0.0), "i"(0)
                 : "cr2");
    Record64("mtfsfi 0", Bits(d), Bits(s_), cr & 0x00F00000);
    switch (field7)
    {
#define MTFSFI(n)                                                                                  \
  case n:                                                                                          \
    asm volatile("mtfsfi 7," #n "\n\tfadd %0,%3,%4\n\tmcrfs 2,7\n\tmfcr %1\n\tfmul %2,%3,%4\n\t"     \
                 "mtfsf 0xff,%5"                                                                   \
                 : "=&f"(d), "=&r"(cr), "=&f"(s_)                                                 \
                 : "f"(a), "f"(1e-310), "f"(0.0)                                                   \
                 : "cr2");                                                                         \
    break;
      MTFSFI(0) MTFSFI(1) MTFSFI(2) MTFSFI(3) MTFSFI(4) MTFSFI(5) MTFSFI(6) MTFSFI(7)
#undef MTFSFI
    }
    Record64("mtfsfi", Bits(d), Bits(s_), cr & 0x00F00000);

    // Instructions that the JIT has the interpreter run, writing registers that it holds in host
    // registers before and uses after.
    double e = a;
    asm volatile("fadd %0,%0,%1\n\tfres %0,%2\n\tfadd %0,%0,%1" : "+f"(e) : "f"(c), "f"(b));
    Record64("fres", Bits(e), 0, 0);
    // fres writes both halves, frsqrte only ps0.
    double e1;
    asm volatile("ps_merge00 %0,%2,%2\n\tfres %0,%3\n\tps_merge11 %1,%0,%0"
                 : "=&f"(e), "=f"(e1)
                 : "f"(c), "f"(b));
    Record64("fres ps1", Bits(e), Bits(e1), 0);
    asm volatile("ps_merge00 %0,%2,%2\n\tfrsqrte %0,%3\n\tps_merge11 %1,%0,%0"
                 : "=&f"(e), "=f"(e1)
                 : "f"(c), "f"(b));
    Record64("frsqrte ps1", Bits(e), Bits(e1), 0);
    const u32 gqr = Random();
    asm volatile("mtspr %1,%0" : : "r"(gqr), "i"(915));
    u32 y = Random(), z;
    asm volatile("addi %0,%0,1\n\tmfspr %0,%2\n\tadd %1,%0,%0" : "+r"(y), "=r"(z) : "i"(915));
    Record("mfspr", y, z, 0);

    // A compare replaces FPSCR[FPCC], which an instruction before it may have set to a class
    // with more than one bit, such as -infinity's.
    asm volatile("mtfsf 0xff,%2\n\tfdiv %0,%3,%4\n\tfcmpu cr2,%4,%3\n\tmffs %1"
                 : "=&f"(d), "=&f"(s_)
                 : "f"(0.0), "f"(a), "f"(b)
                 : "cr2");
    Record64("fdiv fcmpu", Bits(d), Bits(s_) & FPSCR_MASK, 0);
    asm volatile("mtfsf 0xff,%2\n\tfdiv %0,%3,%4\n\tps_cmpo0 cr2,%3,%4\n\tmffs %1"
                 : "=&f"(d), "=&f"(s_)
                 : "f"(0.0), "f"(a), "f"(b)
                 : "cr2");
    Record64("fdiv ps_cmpo0", Bits(d), Bits(s_) & FPSCR_MASK, 0);
  }
  End("fpscr-record-forms");
}

static void TestPairedSingleInputs(void)
{
  static const u32 ties[][3] = {
      // a, b, c
      {0x42480000, 0x1b1c72a0, 0xbc88cc38}, {0xc1b4fd47, 0x29ff8325, 0xbf019c77},
      {0xc2118627, 0xb47a1e02, 0xc13b95b2}, {0xbc790257, 0xaabfede6, 0xc3471e58},
      {0xbd881ae9, 0x2a9fca37, 0xc289d2bd}, {0x4280dc53, 0xb3d01405, 0x416a9cb9},
  };
  const u32 count = sizeof(ties) / sizeof(ties[0]);
  // a, b and c for half 0 are inputs[0], [2] and [4]; for half 1, [1], [3] and [5].
  u32 inputs[6] __attribute__((aligned(8)));

  Begin();
  {
    const double fpscr = Double(0);
    for (u32 i = 0; i < 3 * count; i++)
    {
      BeginIteration();
      // A tie in half 0, in half 1, or in both, and ordinary values in the other.
      const u32 where = i % 3;
      for (u32 h = 0; h < 2; h++)
      {
        const bool tie = where == 2 || where == h;
        for (u32 k = 0; k < 3; k++)
          inputs[2 * k + h] = tie ? ties[(i / 3 + h) % count][k] : (0x3F800000 | (Random() & 0x7FFFFF));
      }
      PSOP("ps_madd %0,1,3,2");
      PSOP("ps_msub %0,1,3,2");
      PSOP("ps_nmadd %0,1,3,2");
      PSOP("ps_nmsub %0,1,3,2");
      PSOP("ps_madds0 %0,1,3,2");
      PSOP("ps_madds1 %0,1,3,2");
      PSOP("ps_mul %0,1,3");
      PSOP("ps_add %0,1,2");
    }
  }
  End("paired-single-ties");

  Begin();
  for (int n = 0; n < 2; n++)
  {
    // NI, and without it, results around the smallest normal single, 2^-126.
    const double fpscr = Double(n ? 4 : 0);
    for (u32 i = 0; i < ITERATIONS / 4; i++)
    {
      BeginIteration();
      for (u32 j = 0; j < 6; j++)
      {
        // Exponents around -63, so that products are around 2^-126; some signs negative.
        const u32 exponent = 127 - 63 - 2 + (Random() % 5);
        inputs[j] = (Random() & 0x807FFFFF) | (exponent << 23);
      }
      // b smaller still, for the sums and differences.
      inputs[2] = (inputs[2] & 0x807FFFFF) | ((127 - 126 + (Random() % 3)) << 23);
      inputs[3] = (inputs[3] & 0x807FFFFF) | ((127 - 127 + (Random() % 3)) << 23);
      PSOP("ps_mul %0,1,3");
      PSOP("ps_muls0 %0,1,3");
      PSOP("ps_muls1 %0,1,3");
      PSOP("ps_madd %0,1,3,2");
      PSOP("ps_nmsub %0,1,3,2");
      PSOP("ps_add %0,2,2");
      PSOP("ps_sub %0,2,1");
      PSOP("ps_div %0,2,3");
      PSOP("ps_sum0 %0,2,3,1");
      PSOP("ps_sum1 %0,2,3,1");
    }
  }
  End("paired-single-ni");

  // A tie, which the JIT leaves to the interpreter, then in the same block a product around the
  // smallest normal single, which FPSCR[NI] flushes or not: NI has to be known on both paths.
  Begin();
  for (u32 i = 0; i < 4 * count; i++)
  {
    BeginIteration();
    const double fpscr = Double(i & 1 ? 4 : 0);
    for (u32 k = 0; k < 3; k++)
    {
      inputs[2 * k] = ties[(i / 2) % count][k];
      inputs[2 * k + 1] = 0x3F800000 | (Random() & 0x7FFFFF);
    }
    u32 tiny[4] __attribute__((aligned(8)));
    for (u32 j = 0; j < 4; j++)
      tiny[j] = (Random() & 0x807FFFFF) | ((127 - 63 - 2 + (Random() % 5)) << 23);
    double r, t, s;
    asm volatile("mtfsf 0xff,%3\n\tpsq_l 1,0(%4),0,0\n\tpsq_l 2,8(%4),0,0\n\t"
                 "psq_l 3,16(%4),0,0\n\tpsq_l 4,0(%5),0,0\n\tpsq_l 5,8(%5),0,0\n\t"
                 "ps_madd %0,1,3,2\n\tps_mul %1,4,5\n\tmffs %2"
                 : "=&f"(r), "=&f"(t), "=&f"(s)
                 : "f"(fpscr), "b"(inputs), "b"(tiny)
                 : "fr1", "fr2", "fr3", "fr4", "fr5", "memory");
    double out[4];
    asm volatile("stfd %2,0(%0)\n\tps_merge11 %2,%2,%2\n\tstfd %2,8(%0)\n\t"
                 "stfd %1,16(%0)\n\tps_merge11 %1,%1,%1\n\tstfd %1,24(%0)"
                 : : "b"(out), "f"(t), "f"(r) : "memory");
    for (u32 j = 0; j < 4; j++)
      Mix64(Bits(out[j]));
    Mix64(Bits(s) & FPSCR_MASK);

    // The same after FPSCR, which a JIT may keep in a host register, was used and then evicted by
    // writing 16 GPRs, so that the tie's instruction is the next to use it.
    asm volatile("mtfsf 0xff,%3\n\tpsq_l 1,0(%4),0,0\n\tpsq_l 2,8(%4),0,0\n\t"
                 "psq_l 3,16(%4),0,0\n\tpsq_l 4,0(%5),0,0\n\tpsq_l 5,8(%5),0,0\n\t"
                 "ps_mul 6,4,5\n\t"
                 "addi 14,14,1\n\t""addi 15,15,1\n\t""addi 16,16,1\n\t""addi 17,17,1\n\t"
                 "addi 18,18,1\n\t""addi 19,19,1\n\t""addi 20,20,1\n\t""addi 21,21,1\n\t"
                 "addi 22,22,1\n\t""addi 23,23,1\n\t""addi 24,24,1\n\t""addi 25,25,1\n\t"
                 "addi 26,26,1\n\t""addi 27,27,1\n\t""addi 28,28,1\n\t""addi 29,29,1\n\t"
                 
                 "ps_madd %0,1,3,2\n\tps_mul %1,4,5\n\tmffs %2"
                 : "=&f"(r), "=&f"(t), "=&f"(s)
                 : "f"(fpscr), "b"(inputs), "b"(tiny)
                 : "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "r14", "r15", "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", "r24", "r25", "r26", "r27", "r28", "r29", "memory");
    asm volatile("stfd %2,0(%0)\n\tps_merge11 %2,%2,%2\n\tstfd %2,8(%0)\n\t"
                 "stfd %1,16(%0)\n\tps_merge11 %1,%1,%1\n\tstfd %1,24(%0)"
                 : : "b"(out), "f"(t), "f"(r) : "memory");
    for (u32 j = 0; j < 4; j++)
      Mix64(Bits(out[j]));
    Mix64(Bits(s) & FPSCR_MASK);
  }
  End("paired-single-tie-ni");
}

// fres and ps_res keep a NaN as it is, as a double, which isn't a single: a multiplicand after them
// is rounded still. fctiwz makes such NaNs, with the integer in the low word.
static void TestEstimateNaNs(void)
{
  Begin();
  for (int i = 0; i < 64; i++)
  {
    BeginIteration();
    const double x = (double)(s32)Value(), a = SingleValue();
    double d, e;
    asm volatile("fctiwz %0,%2\n\tfres %0,%0\n\tfmuls %0,%3,%0" : "=&f"(d), "=&f"(e) : "f"(x), "f"(a));
    Record64("fres fmuls", Bits(d), 0, 0);
    asm volatile("fctiwz %0,%2\n\tfres %0,%0\n\tps_mul %1,%3,%0" : "=&f"(d), "=&f"(e) : "f"(x), "f"(a));
    Record64("fres ps_mul", Bits(e), 0, 0);
    asm volatile("fctiwz %0,%2\n\tps_res %0,%0\n\tps_muls1 %1,%3,%0" : "=&f"(d), "=&f"(e) : "f"(x), "f"(a));
    Record64("ps_res ps_muls1", Bits(e), 0, 0);
    asm volatile("fctiwz %0,%2\n\tps_res %0,%0\n\tfmadds %1,%3,%0,%3" : "=&f"(d), "=&f"(e) : "f"(x), "f"(a));
    Record64("ps_res fmadds", Bits(e), 0, 0);
  }
  End("estimate-nans");
}

static void TestPairedFusedMultiplyAddCases(void)
{
  static const u32 ties[][3] = {
      // a, b, c
      {0x42480000, 0x1b1c72a0, 0xbc88cc38}, {0xc1b4fd47, 0x29ff8325, 0xbf019c77},
      {0xc2118627, 0xb47a1e02, 0xc13b95b2}, {0xbc790257, 0xaabfede6, 0xc3471e58},
      {0xbd881ae9, 0x2a9fca37, 0xc289d2bd}, {0x4280dc53, 0xb3d01405, 0x416a9cb9},
  };
  static const u64 nans[] = {
      0x7FF800000FFFFFFF, 0x7FF4000008000000, 0xFFF000000FFFFFFF, 0x7FF0000000000001,
  };
  const u32 count = sizeof(ties) / sizeof(ties[0]);
  double inputs[6] __attribute__((aligned(8)));
  Begin();
  const double fpscr = Double(0);
  for (u32 i = 0; i < count; i++)
  {
    BeginIteration();
    // a, b and c for half 0 are inputs[0], [2] and [4]; for half 1, [1], [3] and [5].
    for (u32 h = 0; h < 2; h++)
      for (u32 k = 0; k < 3; k++)
        inputs[2 * k + h] = FloatBits(ties[(i + h) % count][k]);
    PSOPD("ps_madd %0,1,3,2");
    PSOPD("ps_msub %0,1,3,2");
    PSOPD("ps_nmadd %0,1,3,2");
    PSOPD("ps_nmsub %0,1,3,2");
    PSOPD("ps_madds0 %0,1,3,2");
    PSOPD("ps_madds1 %0,1,3,2");
  }
  for (u32 i = 0; i < 2 * sizeof(nans) / sizeof(nans[0]); i++)
  {
    BeginIteration();
    inputs[0] = (i & 1) ? 1.0 : 0.0;
    inputs[1] = (i & 1) ? 0.0 : 1.0;
    inputs[2] = inputs[3] = 2.0;
    inputs[4] = Double(nans[i / 2]);
    inputs[5] = Double(nans[(i / 2 + 1) % 4]);
    PSOPD("ps_madd %0,1,3,2");
    PSOPD("ps_nmsub %0,1,3,2");
    PSOPD("ps_madds1 %0,1,3,2");
    PSOPD("ps_mul %0,1,3");
    PSOPD("ps_muls0 %0,1,3");
  }
  End("paired-fma-cases");
}

#define GQR(n) (912 + (n))

static void TestQuantizedLoadsAndStores(void)
{
  Begin();
  static u8 buffer[64] __attribute__((aligned(32)));
  for (int i = 0; i < ITERATIONS; i++)
  {
    for (u32 j = 0; j < sizeof(buffer); j += 4)
      *(u32*)&buffer[j] = Random();
    // Load and store types: float, u8, u16, s8, s16; random scales.
    static const u32 types[] = {0, 4, 5, 6, 7};
    const u32 load_type = types[Random() % 5], store_type = types[Random() % 5];
    const u32 gqr = ((Random() % 64) << 24) | (load_type << 16) | ((Random() % 64) << 8) | store_type;
    // Also at addresses that aren't 8-byte aligned, and through the uncached mirror, which the JIT
    // accesses through the MMU.
    u8* base = &buffer[(Random() % 4) * 3];
    if (Random() % 4 == 0)
      base = (u8*)((u32)base | 0x40000000);
    double ps0, ps1;
    asm volatile("mtspr %3,%2\n\tpsq_l 1,0(%4),0,1\n\tpsq_st 1,32(%4),0,1\n\t"
                 "psq_l 2,8(%4),1,1\n\tpsq_st 2,40(%4),1,1\n\tfmr %0,1\n\tps_merge11 %1,1,1"
                 : "=&f"(ps0), "=&f"(ps1)
                 : "r"(gqr), "i"(GQR(1)), "b"(base)
                 : "fr1", "fr2", "memory");
    Mix64(Bits(ps0));
    Mix64(Bits(ps1));
    for (u32 j = 32; j < 64; j += 4)
      Mix(*(u32*)&buffer[j]);
  }
  End("quantized");

  // Stores of values that the loads don't give, through the routines: NaNs, infinities, values
  // out of the types' ranges, negatives for unsigned types, and doubles just below an integer, in
  // each rounding mode.
  static const double specials[] = {
      0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 1.5, 2.5, -2.5, 127.0, 128.0, -128.0, -129.0, 255.0, 256.0,
      32767.0, 32768.0, -32768.0, -32769.0, 65535.0, 65536.0, 1e30, -1e30, 1e300, -1e300,
      __builtin_inf(), -__builtin_inf(), __builtin_nan(""), -__builtin_nan(""), 0x1p-1060,
      3.0 - 0x1p-30, -3.0 + 0x1p-30, 100.0 - 0x1p-40, 0.99999999, -0.99999999,
  };
  const u32 nspecials = sizeof(specials) / sizeof(specials[0]);
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    static const u32 types[] = {0, 4, 5, 6, 7};
    const u32 store_type = types[Random() % 5];
    const u32 gqr = ((Random() % 64) << 8) | store_type;
    const double v[2] = {specials[Random() % nspecials], specials[Random() % nspecials]};
    for (u32 j = 32; j < 48; j += 4)
      *(u32*)&buffer[j] = 0;
    asm volatile("mtfsf 0xff,%0\n\tmtspr %2,%1\n\tlfd 1,0(%3)\n\tlfd 2,8(%3)\n\t"
                 "ps_merge00 1,1,2\n\tpsq_st 1,32(%4),0,1\n\tpsq_st 1,40(%4),1,1\n\t"
                 "mtfsf 0xff,%5"
                 :
                 : "f"(Double(i % 4)), "r"(gqr), "i"(GQR(1)), "b"(v), "b"(buffer),
                   "f"(Double(0))
                 : "fr1", "fr2", "memory");
    for (u32 j = 32; j < 48; j += 4)
      Mix(*(u32*)&buffer[j]);
  }
  End("quantized-special-stores");
}

// Quantized loads and stores with GQRs that are set outside the function that uses them, so that
// the JITs can compile them for their types and scales, and in all forms.
static void __attribute__((noipa)) QuantizedOps(u8* buf, const double* values, double* out,
                                                u8* stores, u32 index, u8** p, u8** q)
{
  u8* p_ = *p;
  u8* q_ = *q;
  asm volatile(
      "psq_l 1,32(%[buf]),0,2\n\t"
      "psq_l 2,40(%[buf]),1,2\n\t"
      "stfd 1,64(%[out])\n\t"
      "ps_merge11 1,1,1\n\t"
      "stfd 1,72(%[out])\n\t"
      "stfd 2,80(%[out])\n\t"
      "ps_merge11 2,2,2\n\t"
      "stfd 2,88(%[out])\n\t"
      "lfd 3,32(%[values])\n\t"
      "lfd 4,40(%[values])\n\t"
      "ps_merge00 3,3,4\n\t"
      "psq_st 3,32(%[stores]),0,2\n\t"
      "psq_st 3,40(%[stores]),1,2\n\t"
      "psq_l 1,48(%[buf]),0,3\n\t"
      "psq_l 2,56(%[buf]),1,3\n\t"
      "stfd 1,96(%[out])\n\t"
      "ps_merge11 1,1,1\n\t"
      "stfd 1,104(%[out])\n\t"
      "stfd 2,112(%[out])\n\t"
      "ps_merge11 2,2,2\n\t"
      "stfd 2,120(%[out])\n\t"
      "lfd 3,48(%[values])\n\t"
      "lfd 4,56(%[values])\n\t"
      "ps_merge00 3,3,4\n\t"
      "psq_st 3,48(%[stores]),0,3\n\t"
      "psq_st 3,56(%[stores]),1,3\n\t"
      "psq_l 1,64(%[buf]),0,4\n\t"
      "psq_l 2,72(%[buf]),1,4\n\t"
      "stfd 1,128(%[out])\n\t"
      "ps_merge11 1,1,1\n\t"
      "stfd 1,136(%[out])\n\t"
      "stfd 2,144(%[out])\n\t"
      "ps_merge11 2,2,2\n\t"
      "stfd 2,152(%[out])\n\t"
      "lfd 3,64(%[values])\n\t"
      "lfd 4,72(%[values])\n\t"
      "ps_merge00 3,3,4\n\t"
      "psq_st 3,64(%[stores]),0,4\n\t"
      "psq_st 3,72(%[stores]),1,4\n\t"
      "psq_l 1,80(%[buf]),0,5\n\t"
      "psq_l 2,88(%[buf]),1,5\n\t"
      "stfd 1,160(%[out])\n\t"
      "ps_merge11 1,1,1\n\t"
      "stfd 1,168(%[out])\n\t"
      "stfd 2,176(%[out])\n\t"
      "ps_merge11 2,2,2\n\t"
      "stfd 2,184(%[out])\n\t"
      "lfd 3,80(%[values])\n\t"
      "lfd 4,88(%[values])\n\t"
      "ps_merge00 3,3,4\n\t"
      "psq_st 3,80(%[stores]),0,5\n\t"
      "psq_st 3,88(%[stores]),1,5\n\t"
      "psq_l 1,96(%[buf]),0,6\n\t"
      "psq_l 2,104(%[buf]),1,6\n\t"
      "stfd 1,192(%[out])\n\t"
      "ps_merge11 1,1,1\n\t"
      "stfd 1,200(%[out])\n\t"
      "stfd 2,208(%[out])\n\t"
      "ps_merge11 2,2,2\n\t"
      "stfd 2,216(%[out])\n\t"
      "lfd 3,96(%[values])\n\t"
      "lfd 4,104(%[values])\n\t"
      "ps_merge00 3,3,4\n\t"
      "psq_st 3,96(%[stores]),0,6\n\t"
      "psq_st 3,104(%[stores]),1,6\n\t"
      "psq_l 1,112(%[buf]),0,7\n\t"
      "psq_l 2,120(%[buf]),1,7\n\t"
      "stfd 1,224(%[out])\n\t"
      "ps_merge11 1,1,1\n\t"
      "stfd 1,232(%[out])\n\t"
      "stfd 2,240(%[out])\n\t"
      "ps_merge11 2,2,2\n\t"
      "stfd 2,248(%[out])\n\t"
      "lfd 3,112(%[values])\n\t"
      "lfd 4,120(%[values])\n\t"
      "ps_merge00 3,3,4\n\t"
      "psq_st 3,112(%[stores]),0,7\n\t"
      "psq_st 3,120(%[stores]),1,7\n\t"
      "psq_lx 1,%[buf],%[index],0,4\n\t"
      "stfd 1,256(%[out])\n\t"
      "ps_merge11 1,1,1\n\t"
      "stfd 1,264(%[out])\n\t"
      "psq_lux 1,%[p],%[index],1,5\n\t"
      "stfd 1,272(%[out])\n\t"
      "ps_merge11 1,1,1\n\t"
      "stfd 1,280(%[out])\n\t"
      "psq_stx 3,%[stores],%[index],0,6\n\t"
      "psq_stux 3,%[q],%[index],1,7\n\t"
      "psq_lu 2,8(%[p]),0,6\n\t"
      "stfd 2,288(%[out])\n\t"
      "ps_merge11 2,2,2\n\t"
      "stfd 2,296(%[out])\n\t"
      "psq_stu 3,16(%[q]),0,2\n\t"
      : [p] "+b"(p_), [q] "+b"(q_)
      : [buf] "b"(buf), [values] "b"(values), [out] "b"(out), [stores] "b"(stores),
        [index] "r"(index)
      : "fr1", "fr2", "fr3", "fr4", "memory");
  *p = p_;
  *q = q_;
}

static void __attribute__((noipa)) SetGQR3(u32 value)
{
  asm volatile("mtspr %0,%1" : : "i"(GQR(3)), "r"(value));
}
static void (*volatile set_gqr3)(u32) = SetGQR3;

static void TestQuantizedCompiled(void)
{
  // GQR2-GQR7: load type and scale, store type and scale, with scales of both signs.
  static const u32 gqrs[] = {
      (0 << 16) | (0 << 24) | (0 << 0) | (0 << 8),    // float
      (4 << 16) | (3 << 24) | (4 << 0) | (5 << 8),    // u8
      (5 << 16) | (60 << 24) | (5 << 0) | (40 << 8),  // u16, negative scales
      (6 << 16) | (31 << 24) | (6 << 0) | (2 << 8),   // s8
      (7 << 16) | (8 << 24) | (7 << 0) | (63 << 8),   // s16
      (7 << 16) | (0 << 24) | (4 << 0) | (0 << 8),    // s16 loads, u8 stores, unscaled
  };
  static u8 buf[512] __attribute__((aligned(32)));
  static u8 stores[512] __attribute__((aligned(32)));
  static double values[16] __attribute__((aligned(16)));
  static double out[48] __attribute__((aligned(16)));
  asm volatile("mtspr %0,%1" : : "i"(GQR(2)), "r"(gqrs[0]));
  asm volatile("mtspr %0,%1" : : "i"(GQR(3)), "r"(gqrs[1]));
  asm volatile("mtspr %0,%1" : : "i"(GQR(4)), "r"(gqrs[2]));
  asm volatile("mtspr %0,%1" : : "i"(GQR(5)), "r"(gqrs[3]));
  asm volatile("mtspr %0,%1" : : "i"(GQR(6)), "r"(gqrs[4]));
  asm volatile("mtspr %0,%1" : : "i"(GQR(7)), "r"(gqrs[5]));
  Begin();
  for (int i = 0; i < ITERATIONS; i++)
  {
    // Change a GQR near the end, so that the JITs find their assumption wrong. The call is
    // indirect, so that the change isn't in the same block as the instructions that use it.
    if (i == ITERATIONS - 100)
      set_gqr3(gqrs[3]);
    for (u32 j = 0; j < sizeof(buf); j += 4)
      *(u32*)&buf[j] = Random();
    for (int j = 0; j < 16; j++)
    {
      // Values in the ranges of the integer types, after scaling, others, and doubles just below
      // an integer, which rounding to single precision first makes that integer.
      const double v = FloatValue();
      const u32 kind = Random() % 3;
      if (kind == 0)
        values[j] = v;
      else if (kind == 1)
        values[j] = (double)(s32)(Random() % 140000 - 70000) / 64.0;
      else
        values[j] = (double)(s32)(Random() % 200 - 99) - 0x1p-30;
    }
    u8* p = buf + 64;
    u8* q = stores + 128;
    const u32 index = 8 * (Random() % 8);
    // Each rounding mode, which rounding to single precision first depends on.
    asm volatile("mtfsf 0xff,%0" : : "f"(Double(i % 4)));
    QuantizedOps(buf, values, out, stores, index, &p, &q);
    asm volatile("mtfsf 0xff,%0" : : "f"(Double(0)));
    for (int j = 0; j < 38; j++)
      Mix64(Bits(out[j]));
    for (u32 j = 0; j < 256; j += 4)
      Mix(*(u32*)&stores[j]);
    Mix((u32)(p - buf));
    Mix((u32)(q - stores));
  }
  End("quantized-compiled");
}

static u32 code[8] __attribute__((aligned(32)));

static void FlushCode(void* start, u32 size)
{
  for (u32 i = 0; i < size; i += 32)
    asm volatile("dcbst 0,%0\n\tsync\n\ticbi 0,%0" : : "r"((u8*)start + i) : "memory");
  asm volatile("sync\n\tisync" : : : "memory");
}

static void TestSelfModifyingCode(void)
{
  Begin();
  u32 (*const function)(void) = (u32(*)(void))code;
  for (u32 i = 0; i < 100; i++)
  {
    code[0] = 0x38600000 | (i * 7);  // li r3, i * 7
    code[1] = 0x4E800020;            // blr
    FlushCode(code, sizeof(code));
    Mix(function());
  }
  End("self-modifying-code");
}

// Exception handlers, copied to the exception vectors. They count exceptions in SPRG1-SPRG3.
extern const u32 program_handler[], program_handler_end[];
extern const u32 syscall_handler[], syscall_handler_end[];
extern const u32 fpu_handler[], fpu_handler_end[];
asm(".section .text\n"
    "program_handler:\n"
    "  mtsprg0 3\n  mfsprg1 3\n  addi 3,3,1\n  mtsprg1 3\n"
    "  mfsrr0 3\n  addi 3,3,4\n  mtsrr0 3\n  mfsprg0 3\n  rfi\n"
    "program_handler_end:\n"
    "syscall_handler:\n"
    "  mtsprg0 3\n  mfsprg2 3\n  addi 3,3,1\n  mtsprg2 3\n  mfsprg0 3\n  rfi\n"
    "syscall_handler_end:\n"
    "fpu_handler:\n"
    "  mtsprg0 3\n  mfsprg3 3\n  addi 3,3,1\n  mtsprg3 3\n"
    "  mfsrr1 3\n  ori 3,3,0x2000\n  mtsrr1 3\n  mfsprg0 3\n  rfi\n"
    "fpu_handler_end:\n");

// Records SRR0 and DAR at 0x800000F0 and skips the instruction.
extern const u32 alignment_handler[], alignment_handler_end[];
asm(".section .text\n"
    "alignment_handler:\n"
    "  mtsprg0 3\n  mfsrr0 3\n  stw 3,0xF0(0)\n  mfdar 3\n  stw 3,0xF4(0)\n"
    "  mfsrr0 3\n  addi 3,3,4\n  mtsrr0 3\n  mfsprg0 3\n  rfi\n"
    "alignment_handler_end:\n");

static void InstallHandler(u32 vector, const u32* start, const u32* end)
{
  u32* const destination = (u32*)(0x80000000 + vector);
  for (const u32* p = start; p < end; p++)
    destination[p - start] = *p;
  FlushCode(destination, 0x100);
}

#ifdef TEST_DSI
// Data storage exceptions, which only MMU mode raises: an empty page table makes every address
// that the BATs don't map fault. The handler records DAR, DSISR and a count in SPRG1-SPRG3, and
// skips the faulting instruction, which must not have written any register.
extern const u32 dsi_handler[], dsi_handler_end[];
asm(".section .text\n"
    "dsi_handler:\n"
    "  mtsprg0 3\n  mfsprg1 3\n  addi 3,3,1\n  mtsprg1 3\n"
    "  mfdar 3\n  mtsprg2 3\n  mfdsisr 3\n  mtsprg3 3\n"
    "  mfsrr0 3\n  addi 3,3,4\n  mtsrr0 3\n  mfsprg0 3\n  rfi\n"
    "dsi_handler_end:\n");

// The page table: 64 KiB of invalid entries, at physical 16 MiB.
#define PAGE_TABLE ((u32*)0x81000000)

static void RecordDSI(const char* name, u32 a, u32 b)
{
  u32 count, dar, dsisr, srr0;
  asm volatile("mfsprg1 %0\n\tmfsprg2 %1\n\tmfsprg3 %2\n\tmfsrr0 %3"
               : "=r"(count), "=r"(dar), "=r"(dsisr), "=r"(srr0));
  Record(name, a, b, dar);
  Mix(count);
  Mix(dsisr);
  Mix(srr0);
}

#define DSI_LOAD(insn, offset)                                                                     \
  do                                                                                               \
  {                                                                                                \
    u32 r_ = value;                                                                                \
    asm volatile(insn " %0," #offset "(%1)" : "+r"(r_) : "b"(bad) : "memory");                     \
    RecordDSI(insn, r_, 0);                                                                        \
  } while (0)
#define DSI_LOADX(insn)                                                                            \
  do                                                                                               \
  {                                                                                                \
    u32 r_ = value;                                                                                \
    asm volatile(insn " %0,%1,%2" : "+r"(r_) : "b"(bad), "r"(index) : "memory");                   \
    RecordDSI(insn, r_, 0);                                                                        \
  } while (0)
#define DSI_LOADU(insn, offset)                                                                    \
  do                                                                                               \
  {                                                                                                \
    u32 r_ = value;                                                                                \
    u8* p_ = bad;                                                                                  \
    asm volatile(insn " %0," #offset "(%1)" : "+r"(r_), "+b"(p_) : : "memory");                    \
    RecordDSI(insn, r_, (u32)(p_ - bad));                                                          \
  } while (0)
#define DSI_LOADUX(insn)                                                                           \
  do                                                                                               \
  {                                                                                                \
    u32 r_ = value;                                                                                \
    u8* p_ = bad;                                                                                  \
    asm volatile(insn " %0,%1,%2" : "+r"(r_), "+b"(p_) : "r"(index) : "memory");                   \
    RecordDSI(insn, r_, (u32)(p_ - bad));                                                          \
  } while (0)
#define DSI_STORE(insn, offset)                                                                    \
  do                                                                                               \
  {                                                                                                \
    asm volatile(insn " %0," #offset "(%1)" : : "r"(value), "b"(bad) : "memory");                  \
    RecordDSI(insn, 0, 0);                                                                         \
  } while (0)
#define DSI_STOREX(insn)                                                                           \
  do                                                                                               \
  {                                                                                                \
    asm volatile(insn " %0,%1,%2" : : "r"(value), "b"(bad), "r"(index) : "memory");                \
    RecordDSI(insn, 0, 0);                                                                         \
  } while (0)
#define DSI_STOREU(insn, offset)                                                                   \
  do                                                                                               \
  {                                                                                                \
    u8* p_ = bad;                                                                                  \
    asm volatile(insn " %1," #offset "(%0)" : "+b"(p_) : "r"(value) : "memory");                   \
    RecordDSI(insn, (u32)(p_ - bad), 0);                                                           \
  } while (0)
#define DSI_FLOAD(insn, offset)                                                                    \
  do                                                                                               \
  {                                                                                                \
    double d_ = fvalue;                                                                            \
    u8* p_ = bad;                                                                                  \
    asm volatile(insn " %0," #offset "(%1)" : "+f"(d_), "+b"(p_) : : "memory");                    \
    const u64 bits_ = Bits(d_);                                                                    \
    RecordDSI(insn, (u32)(bits_ >> 32) ^ (u32)bits_, (u32)(p_ - bad));                             \
  } while (0)
#define DSI_FSTORE(insn, offset)                                                                   \
  do                                                                                               \
  {                                                                                                \
    u8* p_ = bad;                                                                                  \
    asm volatile(insn " %1," #offset "(%0)" : "+b"(p_) : "f"(fvalue) : "memory");                  \
    RecordDSI(insn, (u32)(p_ - bad), 0);                                                           \
  } while (0)

// Loads and stores of every kind at bad: unaligned at bad + index.
static void DSIAccesses(u8* const bad, const u32 index, const u32 value, const double fvalue)
{
  DSI_LOAD("lbz", 1);
  DSI_LOAD("lhz", 2);
  DSI_LOAD("lha", -2);
  DSI_LOAD("lwz", 4);
  DSI_LOADX("lwzx");
  DSI_LOADX("lhbrx");
  DSI_LOADX("lwbrx");
  DSI_LOADU("lbzu", 1);
  DSI_LOADU("lhau", 2);
  DSI_LOADU("lwzu", 8);
  DSI_LOADUX("lwzux");
  DSI_STORE("stb", 1);
  DSI_STORE("sth", 2);
  DSI_STORE("stw", 4);
  DSI_STOREX("stwx");
  DSI_STOREX("sthbrx");
  DSI_STOREU("stbu", 1);
  DSI_STOREU("stwu", 8);
  DSI_FLOAD("lfs", 4);
  DSI_FLOAD("lfd", 8);
  DSI_FLOAD("lfsu", 4);
  DSI_FLOAD("lfdu", 8);
  DSI_FSTORE("stfs", 4);
  DSI_FSTORE("stfd", 8);
  DSI_FSTORE("stfdu", 8);

  // Quantized, with GQR0, whose types are float.
  {
    double ps0 = fvalue, ps1;
    u8* p_ = bad;
    asm volatile("ps_merge00 %0,%0,%0\n\tpsq_lu %0,8(%2),0,0\n\tps_merge11 %1,%0,%0"
                 : "+f"(ps0), "=f"(ps1), "+b"(p_)
                 :
                 : "memory");
    RecordDSI("psq_lu", (u32)Bits(ps0) ^ (u32)(Bits(ps1) >> 32), (u32)(p_ - bad));
    p_ = bad;
    asm volatile("psq_stu %1,8(%0),0,0" : "+b"(p_) : "f"(fvalue) : "memory");
    RecordDSI("psq_stu", (u32)(p_ - bad), 0);
  }

  // Quantized, with a GQR that the block sets, so that the JIT doesn't know its type.
  {
    static const u32 types[] = {0, 4, 5, 6, 7};
    const u32 gqr = (types[Random() % 5] << 16) | types[Random() % 5];
    double ps0 = fvalue, ps1;
    u8* p_ = bad;
    asm volatile("mtspr %4,%3\n\tps_merge00 %0,%0,%0\n\tpsq_lu %0,8(%2),0,1\n\t"
                 "ps_merge11 %1,%0,%0"
                 : "+f"(ps0), "=f"(ps1), "+b"(p_)
                 : "r"(gqr), "i"(GQR(1))
                 : "memory");
    RecordDSI("psq_lu gqr", (u32)Bits(ps0) ^ (u32)(Bits(ps1) >> 32), (u32)(p_ - bad));
    p_ = bad;
    asm volatile("mtspr %3,%2\n\tpsq_stu %1,8(%0),0,1"
                 : "+b"(p_)
                 : "f"(fvalue), "r"(gqr), "i"(GQR(1))
                 : "memory");
    RecordDSI("psq_stu gqr", (u32)(p_ - bad), 0);
  }

  asm volatile("dcbz 0,%0" : : "b"(bad) : "memory");
  RecordDSI("dcbz", 0, 0);
  // lwarx and stwcx. need aligned addresses.
  {
    u32 r_ = value, cr;
    asm volatile("lwarx %0,%1,%2" : "+r"(r_) : "b"(bad), "r"(index & ~3) : "memory");
    RecordDSI("lwarx", r_, 0);
    asm volatile("stwcx. %1,%2,%3\n\tmfcr %0"
                 : "=r"(cr)
                 : "r"(value), "b"(bad), "r"(index & ~3)
                 : "cr0", "memory");
    RecordDSI("stwcx.", cr >> 28, 0);
  }

  // Load and store multiple words: the registers keep their values.
  {
    u32 words[4] = {value, value + 1, value + 2, value + 3};
    u8* const p_ = bad;
    asm volatile("mr 28,%0\n\tmr 29,%1\n\tmr 30,%2\n\tmr 31,%3\n\tlmw 28,0(%4)\n\t"
                 "mr %0,28\n\tmr %1,29\n\tmr %2,30\n\tmr %3,31"
                 : "+r"(words[0]), "+r"(words[1]), "+r"(words[2]), "+r"(words[3])
                 : "b"(p_)
                 : "r28", "r29", "r30", "r31", "memory");
    RecordDSI("lmw", words[0] ^ words[1], words[2] ^ words[3]);
    asm volatile("mr 28,%0\n\tmr 29,%1\n\tmr 30,%0\n\tmr 31,%1\n\tstmw 28,0(%2)"
                 :
                 : "r"(value), "r"(index), "b"(p_)
                 : "r28", "r29", "r30", "r31", "memory");
    RecordDSI("stmw", 0, 0);
  }

  // A fault in the middle of a block: what the instructions before and after it wrote must
  // be kept, and what it would have written must not.
  {
    u32 a = value, b = value ^ 0x5A5A5A5A;
    double f = fvalue, g = fvalue;
    asm volatile("addi %0,%0,1\n\tfadd %2,%2,%2\n\tlwz %1,0(%4)\n\taddi %0,%0,2\n\t"
                 "lfd %3,0(%4)\n\tfadd %2,%2,%2"
                 : "+r"(a), "+r"(b), "+f"(f), "+f"(g)
                 : "b"(bad)
                 : "memory");
    RecordDSI("mid-block", a, b);
    Mix64(Bits(f));
    Mix64(Bits(g));
  }
}

static void TestDSI(void)
{
  Begin();
  InstallHandler(0x300, dsi_handler, dsi_handler_end);
  asm volatile("li 3,0\n\tmtsprg1 3\n\tmtsprg2 3\n\tmtsprg3 3" : : : "r3");

  u32 sdr1;
  asm volatile("mfsdr1 %0" : "=r"(sdr1));
  for (u32 i = 0; i < 0x10000 / 4; i++)
    PAGE_TABLE[i] = 0;
  FlushCode(PAGE_TABLE, 0x10000);
  asm volatile("mtsdr1 %0\n\tsync\n\ttlbsync\n\tsync" : : "r"((u32)PAGE_TABLE & 0x3FFFFFFF));
  for (u32 page = 0; page < 0x100000; page += 0x1000)
    asm volatile("tlbie %0" : : "r"(page));
  asm volatile("sync\n\ttlbsync\n\tsync");

  for (int i = 0; i < 50; i++)
  {
    BeginIteration();
    u8* const bad = (u8*)(0x40000000 + (Random() & 0xFFFFF0));
    const u32 index = Random() % 4096;
    const u32 value = Value();
    const double fvalue = FloatValue();

    DSIAccesses(bad, index, value, fvalue);
  }

  asm volatile("mtsdr1 %0\n\tsync" : : "r"(sdr1));
  u32 count;
  asm volatile("mfsprg1 %0" : "=r"(count));
  OSReport("TEST dsi count %u\n", count);
  End("dsi");
}

// Translation through the page table, which the JIT does itself for addresses in the data TLB:
// accesses of every kind to mapped pages, uncached ones and BAT-mapped uncached RAM, crossing into
// a page that may not be mapped, the R and C bits, which entry of a set the TLB replaces, and its
// stale entries until tlbie.
#define PT_SEGMENT 0x60000000
// The pages that the test maps, which it reads through the cached BAT.
#define PT_PHYSICAL 0x01100000
#define PT_PAGES 16

static u32* PTE(u32 vsid, u32 ea, u32 slot)
{
  const u32 hash = vsid ^ ((ea >> 12) & 0xFFFF);
  return PAGE_TABLE + ((hash << 6) & 0xFFC0) / 4 + slot * 2;
}

static void MapPage(u32 vsid, u32 ea, u32 slot, u32 pa, u32 bits)
{
  u32* const pte = PTE(vsid, ea, slot);
  pte[0] = 0x80000000 | (vsid << 7) | ((ea >> 22) & 0x3F);
  pte[1] = pa | bits | 2;
  asm volatile("sync\n\ttlbie %0\n\tsync\n\ttlbsync\n\tsync" : : "r"(ea) : "memory");
}

static void UnmapPage(u32 vsid, u32 ea, u32 slot)
{
  u32* const pte = PTE(vsid, ea, slot);
  pte[0] = 0;
  pte[1] = 0;
  asm volatile("sync\n\ttlbie %0\n\tsync\n\ttlbsync\n\tsync" : : "r"(ea) : "memory");
}

// A load that raises a DSI exception leaves its register as it was: 0.
static u32 LoadWord(u32 ea)
{
  u32 value;
  asm volatile("li %0,0\n\tlwz %0,0(%1)" : "=&r"(value) : "b"(ea) : "memory");
  return value;
}

static void MixPhysicalPages(void)
{
  const u32* const words = (const u32*)(0x80000000 | PT_PHYSICAL);
  for (u32 i = 0; i < PT_PAGES * 0x1000 / 4; i++)
    Mix(words[i]);
}

static void TestPageTable(void)
{
  Begin();
  u32 tlb_set_hash = 0;
  u32 sdr1, sr6;
  asm volatile("mfsdr1 %0\n\tmfsr %1,6" : "=r"(sdr1), "=r"(sr6));
  for (u32 i = 0; i < 0x10000 / 4; i++)
    PAGE_TABLE[i] = 0;
  asm volatile("mtsdr1 %0\n\tsync" : : "r"((u32)PAGE_TABLE & 0x3FFFFFFF));
  u32* const physical = (u32*)(0x80000000 | PT_PHYSICAL);
  for (u32 i = 0; i < PT_PAGES * 0x1000 / 4; i++)
    physical[i] = Random();

  // Attributes: C, R, uncached (I), write-through (W), and none.
  static const u32 attributes[] = {0, 0x80, 0x100, 0x180, 0x20, 0xA0, 0x40, 0x1C0};
  for (int i = 0; i < 60; i++)
  {
    BeginIteration();
    const u32 vsid = (Random() & 1) ? 0x123456 : 0x0ABCDE;
    asm volatile("mtsr 6,%0\n\tisync" : : "r"(vsid));

    // A page, and the next one, mapped three times out of four.
    const u32 ea = PT_SEGMENT | (Random() & 0x0FFFE000);
    const u32 pa = PT_PHYSICAL + (Random() % PT_PAGES) * 0x1000;
    const u32 next_pa = PT_PHYSICAL + (Random() % PT_PAGES) * 0x1000;
    const bool next_mapped = (Random() & 3) != 0;
    MapPage(vsid, ea, 0, pa, attributes[Random() % 8]);
    if (next_mapped)
      MapPage(vsid, ea + 0x1000, 1, next_pa, attributes[Random() % 8]);

    // Anywhere in the page, or at its end, word-aligned.
    const u32 offset = (Random() & 1) ? (Random() & 0xFF0) : 0x1000 - 4 * (1 + Random() % 4);
    const u32 value = Value();
    const double fvalue = FloatValue();
    DSIAccesses((u8*)(ea + offset), Random() % 16, value, fvalue);
    // Uncached RAM through a BAT, whose stores of less than a word write differently.
    DSIAccesses((u8*)(0xC0000000 | (pa + (offset & 0x7F0))), Random() % 16, value, fvalue);

    // Stores of less than a word, or unaligned, to an uncached page write their doubleword
    // differently: read it back before other stores overwrite it.
    {
      const u32 line = ea + (Random() & 0xFF0);
      u32 w0, w1, w2, w3;
      asm volatile("stb %4,1(%5)\n\tlwz %0,0(%5)\n\tlwz %1,4(%5)\n\t"
                   "sth %4,10(%5)\n\tlwz %2,8(%5)\n\tlwz %3,12(%5)"
                   : "=&r"(w0), "=&r"(w1), "=&r"(w2), "=&r"(w3)
                   : "r"(value), "b"(line)
                   : "memory");
      Record("uncached stb sth", w0 ^ w1, w2, w3);
      asm volatile("stw %4,6(%5)\n\tlwz %0,0(%5)\n\tlwz %1,4(%5)\n\tlwz %2,8(%5)\n\t"
                   "lwz %3,12(%5)"
                   : "=&r"(w0), "=&r"(w1), "=&r"(w2), "=&r"(w3)
                   : "r"(~value), "b"(line)
                   : "memory");
      Record("uncached stw", w0 ^ w1, w2, w3);
    }

    u32* const pte = PTE(vsid, ea, 0);
    u32* const next_pte = PTE(vsid, ea + 0x1000, 1);
    Record("pte", pte[1], next_mapped ? next_pte[1] : 0, LoadWord(ea + (Random() & 0xFFC)));

    // A remapped page reads the old page until tlbie.
    pte[1] = (pte[1] & 0xFFF) | (PT_PHYSICAL + (Random() % PT_PAGES) * 0x1000);
    const u32 stale = LoadWord(ea);
    asm volatile("sync\n\ttlbie %0\n\tsync\n\ttlbsync\n\tsync" : : "r"(ea) : "memory");
    Record("remap", stale, LoadWord(ea), pte[1]);

    // Another segment: the TLB has the page with the other VSID.
    asm volatile("mtsr 6,%0\n\tisync" : : "r"(vsid ^ 0x1000));
    u32 count_before, count_after;
    asm volatile("mfsprg1 %0" : "=r"(count_before));
    const u32 other = LoadWord(ea);
    asm volatile("mfsprg1 %0" : "=r"(count_after));
    asm volatile("mtsr 6,%0\n\tisync" : : "r"(vsid));
    Record("segment", other, count_after - count_before, 0);

    // Three pages in one set of the TLB, which has two ways, used in some order, then all remapped
    // without tlbie: which ones still read their old page shows which entries the TLB kept. With
    // page table fastmem, all of them do, so this is reported apart, to compare with it off.
    u32 set_eas[3];
    for (u32 k = 0; k < 3; k++)
    {
      set_eas[k] = PT_SEGMENT | ((k + 1) << 24) | (Random() & 0x00FC0000) | (ea & 0x3F000);
      MapPage(vsid, set_eas[k], 2 + k, PT_PHYSICAL + (Random() % PT_PAGES) * 0x1000, 0x180);
    }
    u32 sequence = 0;
    for (u32 k = 0; k < 6; k++)
      Mix(sequence += LoadWord(set_eas[Random() % 3] + (Random() & 0xFFC)));
    for (u32 k = 0; k < 3; k++)
    {
      u32* const set_pte = PTE(vsid, set_eas[k], 2 + k);
      set_pte[1] = (set_pte[1] & 0xFFF) | (PT_PHYSICAL + (Random() % PT_PAGES) * 0x1000);
    }
    for (u32 k = 0; k < 3; k++)
      tlb_set_hash = (tlb_set_hash ^ LoadWord(set_eas[k])) * 0x01000193;

    for (u32 k = 0; k < 3; k++)
      UnmapPage(vsid, set_eas[k], 2 + k);
    UnmapPage(vsid, ea, 0);
    if (next_mapped)
      UnmapPage(vsid, ea + 0x1000, 1);
  }

  // The JIT's own translation, once each: a store to a page whose C bit isn't set yet sets it, a
  // page that tlbie unmapped faults, a page mapped to MMIO, PI's revision register, reads it, and
  // a real-mode access ignores the page table.
  {
    const u32 vsid = 0x123456;
    const u32 ea = PT_SEGMENT | 0x00345000;
    asm volatile("mtsr 6,%0\n\tisync" : : "r"(vsid));
    MapPage(vsid, ea, 0, PT_PHYSICAL, 0x100);
    asm volatile("stw %0,16(%1)" : : "r"(Value()), "b"(ea) : "memory");
    Mix(PTE(vsid, ea, 0)[1]);
    Mix(LoadWord(ea + 16));
    UnmapPage(vsid, ea, 0);
    Mix(LoadWord(ea + 16));

    MapPage(vsid, ea, 0, 0x0C003000, 0x180);
    Mix(LoadWord(ea + 0x2C));
    UnmapPage(vsid, ea, 0);

    u32 sr0, msr, value;
    asm volatile("mfsr %0,0" : "=r"(sr0));
    MapPage(sr0 & 0xFFFFFF, 0x0C003000, 0, PT_PHYSICAL, 0x180);
    asm volatile("mfmsr %0\n\trlwinm %1,%0,0,28,26\n\tmtmsr %1\n\tisync\n\tlwz %1,0x2C(%2)\n\t"
                 "mtmsr %0\n\tisync"
                 : "=&r"(msr), "=&r"(value)
                 : "b"(0x0C003000)
                 : "memory");
    Mix(value);
    UnmapPage(sr0 & 0xFFFFFF, 0x0C003000, 0);
  }
  MixPhysicalPages();

  asm volatile("mtsr 6,%0\n\tmtsdr1 %1\n\tsync\n\tisync" : : "r"(sr6), "r"(sdr1));
  u32 count;
  asm volatile("mfsprg1 %0" : "=r"(count));
  OSReport("TEST page table dsi count %u\n", count);
  End("page-table");
  OSReport("TEST page-table tlb set %08x\n", tlb_set_hash);
}
#endif

// A misaligned lmw or stmw raises an alignment exception, which the interpreter takes at the end
// of its block, and writes no register or memory.
static void TestAlignment(void)
{
  Begin();
  InstallHandler(0x600, alignment_handler, alignment_handler_end);
  volatile u32* const record = (volatile u32*)0x800000F0;
  for (u32 i = 0; i < 8; i++)
  {
    u32 buffer[4] = {Value(), Value(), Value(), Value()};
    const u32 seed = Value();
    u32 after, start;
    record[0] = record[1] = 0;
    if (i & 1)
    {
      asm volatile("mr 30,%2\n\tmr 31,%2\n\tlis %1,1f@ha\n\taddi %1,%1,1f@l\n"
                   "1:\tstmw 30,2(%3)\n\taddi %0,30,7"
                   : "=&r"(after), "=&r"(start)
                   : "r"(seed), "b"(buffer)
                   : "r30", "r31", "memory");
    }
    else
    {
      asm volatile("mr 30,%2\n\tmr 31,%2\n\tlis %1,1f@ha\n\taddi %1,%1,1f@l\n"
                   "1:\tlmw 30,1(%3)\n\taddi %0,30,7"
                   : "=&r"(after), "=&r"(start)
                   : "r"(seed), "b"(buffer)
                   : "r30", "r31", "memory");
    }
    Record(i & 1 ? "stmw" : "lmw", after, record[0] - start, record[1] - (u32)buffer);
    for (u32 j = 0; j < 4; j++)
      Mix(buffer[j]);
  }
  End("alignment");
}

static void TestExceptions(void)
{
  Begin();
  InstallHandler(0x700, program_handler, program_handler_end);
  InstallHandler(0xC00, syscall_handler, syscall_handler_end);
  InstallHandler(0x800, fpu_handler, fpu_handler_end);
  asm volatile("li 3,0\n\tmtsprg1 3\n\tmtsprg2 3\n\tmtsprg3 3" : : : "r3");

  for (u32 i = 0; i < 20; i++)
  {
    const u32 a = Random() % 4;
    // Traps: unconditional, and conditional on a, signed and unsigned.
    asm volatile("trap\n\ttwi 4,%0,1\n\ttwi 8,%0,2\n\ttwi 16,%0,3\n\ttw 31,%0,%0\n\t"
                 "twi 2,%0,2\n\ttwi 1,%0,1\n\ttw 3,%0,%1"
                 :
                 : "r"(a), "r"(Random() % 4));
    asm volatile("sc" : : : "memory");
    if (i % 5 == 0)
    {
      // Disable the FPU, so that the next floating-point instruction raises an exception.
      u32 msr;
      double d = (double)a;
      asm volatile("mfmsr %0\n\trlwinm %0,%0,0,19,17\n\tmtmsr %0\n\tisync\n\tfadd %1,%1,%1"
                   : "=&r"(msr), "+f"(d));
      Mix64(Bits(d));
    }
  }

  {
    // dcbz_l zeroes a line of the locked cache, which HID2[LCE] enables and a DBAT maps at
    // 0xE0000000. (Without HID2[LCE], it's illegal: the interpreter defers that exception, which
    // the JIT raises after the instruction, as the hardware does.)
    u32 hid2, dbat3u, dbat3l;
    asm volatile("mfspr %0,920\n\tmfspr %1,542\n\tmfspr %2,543"
                 : "=r"(hid2), "=r"(dbat3u), "=r"(dbat3l));
    asm volatile("mtspr 543,%1\n\tmtspr 542,%0\n\tisync\n\tmtspr 920,%2\n\tisync"
                 :
                 : "r"(0xE0000006), "r"(0xE0000002), "r"(hid2 | 0x10000000));
    volatile u32* const locked = (volatile u32*)(0xE0000000 + (Random() & 0x3FFE0));
    for (u32 j = 0; j < 16; j++)
      locked[j] = Random();
    asm volatile("dcbz_l 0,%0" : : "b"(locked + 8) : "memory");
    for (u32 j = 0; j < 16; j++)
      Mix(locked[j]);
    asm volatile("mtspr 920,%0\n\tisync\n\tmtspr 542,%1\n\tmtspr 543,%2\n\tisync"
                 :
                 : "r"(hid2), "r"(dbat3u), "r"(dbat3l));
  }

  u32 programs, syscalls, fpus;
  asm volatile("mfsprg1 %0\n\tmfsprg2 %1\n\tmfsprg3 %2" : "=r"(programs), "=r"(syscalls), "=r"(fpus));
  Mix(programs);
  Mix(syscalls);
  Mix(fpus);
  OSReport("exceptions: %u program, %u system call, %u FPU unavailable\n", programs,
                  syscalls, fpus);
  End("exceptions");
}

#ifdef TEST_TIMING
// Timing, which depends on where a CPU core's blocks end, since each subtracts its cycles from the
// downcount at its end: the JITs' blocks are longer than the interpreters'. So compare a JIT's
// report with one from a build known to be good, for changes that shouldn't change its timing. It
// checks that the downcount, which a JIT may keep elsewhere while its code runs, is in
// PowerPCState when C++ code reads and changes it: the VI's beam position depends on the cycles
// run so far, and setting the decrementer makes CoreTiming check for its interrupt sooner, by
// lowering the downcount. The handler counts the interrupts in SPRG1, and records where they came
// in SPRG2.
extern const u32 dec_handler[], dec_handler_end[];
asm(".section .text\n"
    "dec_handler:\n"
    "  mtsprg0 3\n  mfsprg1 3\n  addi 3,3,1\n  mtsprg1 3\n  mfsrr0 3\n  mtsprg2 3\n  mfsprg0 3\n"
    "  rfi\n"
    "dec_handler_end:\n");

static void TestTiming(void)
{
  Begin();
  InstallHandler(0x900, dec_handler, dec_handler_end);
  // No external interrupts, which would need a handler.
  *(volatile u32*)0xCC003004 = 0;
  volatile u16* const beam = (volatile u16*)0xCC00202C;
  for (int i = 0; i < 500; i++)
  {
    BeginIteration();
    // Some cycles in JIT code without calls into C++, then the beam position.
    u32 x = Random();
    asm volatile("mtctr %1\n1:\taddi %0,%0,1\n\tbdnz 1b" : "+r"(x) : "r"(1 + Random() % 64) : "ctr");
    const u32 position = (beam[0] << 16) | beam[1];
    // The iterations of a loop until the decrementer interrupt, which the handler counts. A
    // function that the loop calls counts them, which a JIT may link to as a host call, checking
    // the downcount first: a conditional call, which the JIT doesn't follow into the function.
    u32 count, msr, interrupts;
    asm volatile("li %0,0\n\tmtsprg1 %0\n\tmfmsr %1\n\tori %1,%1,0x8000\n\t"
                 "mtdec %3\n\tmtmsr %1\n"
                 "1:\tmfsprg1 %2\n\tcmpwi %2,0\n\tbne 2f\n\tbeql 3f\n\tb 1b\n"
                 "3:\taddi %0,%0,1\n\tblr\n"
                 "2:\tmfmsr %1\n\trlwinm %1,%1,0,17,15\n\tmtmsr %1"
                 : "=&b"(count), "=&r"(msr), "=&r"(interrupts)
                 : "r"(100 + Random() % 5000)
                 : "cr0", "lr");
    // And where it came, which is in the function if the JIT checked the downcount before the call.
    u32 srr0;
    asm volatile("mfsprg2 %0" : "=r"(srr0));
    Record("timing", position, count, x ^ srr0);
  }
  End("timing");
}
#endif

int main(void)
{
  // Enable paired singles and quantized loads and stores (HID2 PSE and LSQE).
  asm volatile("mfspr 3,920\n\toris 3,3,0xA000\n\tmtspr 920,3" : : : "r3");

  OSReport("START\n");
#ifdef TEST_TIMING
  TestTiming();
#else
  TestIntegerArithmetic();
  TestIntegerForms();
  TestIntegerLogic();
  TestRotates();
  TestCompareAndConditionRegister();
  TestLoadsAndStores();
  TestMMIO();
  TestSlowPaths();
  TestReservationsAndSegments();
  TestBranches();
  TestCallsAndReturns();
  TestRegisterPressure();
  TestCREviction();
  TestCompareAndBranch();
  TestFloatingPoint(0);
  TestFloatingPoint(4);  // FPSCR.NI
  TestFusedMultiplyAddCases();
  TestFloatLoadsAndStores();
  TestPairedSingles();
  TestPairedFusedMultiplyAddCases();
  TestPairedSingleInputs();
  TestEstimateNaNs();
  TestFPSCRAndRecordForms();
  TestFPSCRInstructions();
  TestFPSCRAndInterpreter();
  TestQuantizedLoadsAndStores();
  TestQuantizedCompiled();
  TestSelfModifyingCode();
  TestExceptions();
  TestAlignment();
#ifdef TEST_DSI
  TestDSI();
  TestPageTable();
#endif
  TestDeepRecursion();
#endif
  // Dolphin's printf takes the first word of the format string for a pointer to a this argument if
  // it looks like an address, as "DONE" does, so start every report with "TEST".
  OSReport("TEST done\n");
  return 0;
}
