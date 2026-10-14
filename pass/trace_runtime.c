#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

extern const char __ir_trace_opcodes[];

/* Single-threaded app.c. Binary format: opcode byte; edge = two opcode bytes.
 * Compression is lossless, in execution order; no sampling or trace limit. */
#define BUFFER_SIZE 65536u
static gzFile instructions;
static gzFile edges;
static unsigned char instruction_buffer[BUFFER_SIZE];
static unsigned char edge_buffer[BUFFER_SIZE];
static unsigned instruction_size;
static unsigned edge_size;

static void fail(const char *message) {
  fprintf(stderr, "IR trace error: %s\n", message);
  _Exit(2);
}

static void flush(gzFile file, const unsigned char *buffer, unsigned *size) {
  if (*size && gzwrite(file, buffer, *size) != (int)*size)
    fail("cannot write trace");
  *size = 0;
}

static void close_trace(void) {
  flush(instructions, instruction_buffer, &instruction_size);
  flush(edges, edge_buffer, &edge_size);
  int instruction_status = gzclose(instructions);
  int edge_status = gzclose(edges);
  if (instruction_status != Z_OK || edge_status != Z_OK)
    fail("cannot close trace");
}

static void initialize(void) {
  const char *directory = getenv("IR_TRACE_DIR");
  char path[4096];
  if (!directory || !*directory)
    directory = ".";
  if (snprintf(path, sizeof(path), "%s/opcodes.json", directory) >= (int)sizeof(path))
    fail("directory path too long");
  FILE *map = fopen(path, "wb");
  if (!map)
    fail("cannot create opcode dictionary (create IR_TRACE_DIR first)");
  if (fputs(__ir_trace_opcodes, map) == EOF || fclose(map))
    fail("cannot write opcode dictionary");
  snprintf(path, sizeof(path), "%s/instructions.bin.gz", directory);
  instructions = gzopen(path, "wb6");
  snprintf(path, sizeof(path), "%s/uses.bin.gz", directory);
  edges = gzopen(path, "wb6");
  if (!instructions || !edges)
    fail("cannot create traces");
  if (atexit(close_trace))
    fail("cannot register trace finalizer");
}

void __ir_trace(uint32_t opcode, const unsigned char *pairs, uint32_t size) {
  if (!instructions)
    initialize();
  instruction_buffer[instruction_size++] = (unsigned char)opcode;
  if (instruction_size == BUFFER_SIZE)
    flush(instructions, instruction_buffer, &instruction_size);
  while (size) {
    unsigned available = BUFFER_SIZE - edge_size;
    unsigned amount = size < available ? size : available;
    memcpy(edge_buffer + edge_size, pairs, amount);
    edge_size += amount;
    pairs += amount;
    size -= amount;
    if (edge_size == BUFFER_SIZE)
      flush(edges, edge_buffer, &edge_size);
  }
}
