#ifndef MOCK_MEMORY_H
#define MOCK_MEMORY_H

typedef struct {
  unsigned char* ram;
  unsigned size;
}
memory_t;

static unsigned peekb(unsigned address, memory_t* memory) {
  return address < memory->size ? memory->ram[address] : 0;
}

static unsigned peek(unsigned address, unsigned num_bytes, void* ud) {
  memory_t* memory = (memory_t*)ud;

  switch (num_bytes) {
    case 1: return peekb(address, memory);

    case 2: return peekb(address, memory) |
      peekb(address + 1, memory) << 8;

    case 4: return peekb(address, memory) |
      peekb(address + 1, memory) << 8 |
      peekb(address + 2, memory) << 16 |
      peekb(address + 3, memory) << 24;
  }

  return 0;
}

#endif /* MOCK_MEMORY_H */
