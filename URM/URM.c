// Unlimited Register Machine simulator
// Nigel Perks
// 2026

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>

#define INST_FIELD (20)  // output field for instruction number and human-readable instruction 

static void fatal(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  exit(EXIT_FAILURE);
}

typedef unsigned Integer;

// NOOP is for a dummy instruction at instruction number 0
// so that instructions can begin at instruction number 1,
// the instruction number in C being the same as for the user,
// and to catch a jump to 0.

enum { NOOP, ZERO, SUCC, COPY, JUMP };

typedef struct {
  char mnemonic;
  int inst;
  unsigned argc;
} MachineInstruction;

static const MachineInstruction machine_instructions[] = {
  { 'N', NOOP, 0 }, // must be at index 0
  { 'Z', ZERO, 1 },
  { 'S', SUCC, 1 },
  { 'C', COPY, 2 }, // C is used for Copy in Open University course M381
  { 'T', COPY, 2 }, // T is used for Transfer in Nigel Cutland's "Computability"
  { 'J', JUMP, 3 },
};

#define MAX_OPERANDS (3)

typedef struct {
  const MachineInstruction* minst;
  Integer argv[MAX_OPERANDS];
} ProgramInstruction;

typedef struct {
  ProgramInstruction* inst;
  size_t ninst;
  size_t allocated;
} Program;

static Program* new_program(void) {
  Program* p = calloc(1, sizeof (Program));
  if (p == NULL)
    fatal("out of memory for program\n");
  // allocate program space to set up the dummy no-op
  p->allocated = 32;
  p->inst = calloc(p->allocated, sizeof p->inst[0]);
  p->inst->minst = &machine_instructions[0]; // NOOP
  p->ninst = 1;
  return p;
}

static void delete_program(Program* p) {
  free(p);
}

static void ensure_space(Program* p) {
  assert(p->ninst <= p->allocated);
  if (p->allocated == 0 || p->ninst == p->allocated) {
    p->allocated = p->allocated ? 2 * p->allocated : 32;
    p->inst = realloc(p->inst, p->allocated * sizeof p->inst[0]);
  }
  assert(p->ninst < p->allocated);
}

static void invalid_line(unsigned lineno, const char* line, const char* msg) {
  fatal("invalid line: %u: %s: %s\n", lineno, line, msg);
}

static unsigned advance(unsigned lineno, const char* line, size_t len, unsigned i, const char* msg) {
  while (i < len && isspace(line[i]))
    i++;
  if (i >= len)
    invalid_line(lineno, line, msg);
  return i;
}

static unsigned match(unsigned lineno, const char* line, size_t len, unsigned i, char token, const char* msg) {
  i = advance(lineno, line, len, i, msg);
  if (line[i] != token)
    invalid_line(lineno, line, msg);
  return i+1;
}    

static unsigned number(unsigned lineno, const char* line, size_t len, unsigned i, Integer *val, const char* msg) {
  i = advance(lineno, line, len, i, msg);
  *val = 0;
  if (!isdigit(line[i]))
    invalid_line(lineno, line, "number expected");
  while (isdigit(line[i])) {
    *val = *val * 10 + line[i] - '0';
    i++;
  }
  return i;
}

static void parse_line(ProgramInstruction* inst, unsigned lineno, const char* line) {
  const size_t len = strlen(line);
  unsigned i = 0;
  // optional instruction number
  i = advance(lineno, line, len, i, "instruction number or mnemonic expected");
  if (isdigit(line[i])) {
    Integer n;
    i = number(lineno, line, len, i, &n, "instruction number expected");
    if (n != lineno)
      invalid_line(lineno, line, "unexpected instruction number");
  }
  // get instruction mnemonic
  i = advance(lineno, line, len, i, "instruction mnemonic expected");
  inst->minst = NULL;
  for (unsigned j = 0; j < sizeof machine_instructions / sizeof machine_instructions[0]; j++) {
    if (line[i] == machine_instructions[j].mnemonic) {
      inst->minst = &machine_instructions[j];
      break;
    }
  }
  if (inst->minst == NULL)
    invalid_line(lineno, line, "unknown instruction");
  i++;
  // begin operands
  i = match(lineno, line, len, i, '(', "open parenthesis expected");
  // parse operands
  unsigned j = 0;
  i = number(lineno, line, len, i, &inst->argv[j], "operand expected");
  j++;
  while (j < inst->minst->argc) {
    i = match(lineno, line, len, i, ',', "comma expected");
    i = number(lineno, line, len, i, &inst->argv[j], "operand expected");
    j++;
  }
  // end operands
  i = match(lineno, line, len, i, ')', "close parenthesis expected");
  // treat any remaining content as a comment
}

static bool read_line(unsigned lineno, char* line, unsigned max, FILE* fp) {
  if (!fgets(line, max, fp))
    return false;
  size_t len = strlen(line);
  if (len == 0 || line[len-1] != '\n')
    fatal("invalid line: %u\n", lineno);
  line[--len] = '\0';
  return true;
}

static Program* load_program(const char* fname) {
  FILE* fp = fopen(fname, "r");
  if (fp == NULL)
    fatal("cannot open program file: %s\n", fname);
  Program* p = new_program();
  char line[128];
  unsigned lineno = 1;
  while (read_line(lineno, line, sizeof line, fp)) {
    ensure_space(p);
    parse_line(&p->inst[p->ninst], lineno, line);
    p->ninst++;
    lineno++;
  }
  fclose(fp);
  return p;
}

static int print_inst(unsigned instno, const ProgramInstruction* inst) {
  int printed = printf("%5u %c(%llu", instno, inst->minst->mnemonic, (unsigned long long) inst->argv[0]);
  for (unsigned i = 1; i < inst->minst->argc; i++)
    printed += printf(",%llu", (unsigned long long) inst->argv[i]);
  fputs(")", stdout);
  printed++;
  return printed;
}

static void list_program(const Program* p) {
  for (unsigned i = 1; i < p->ninst; i++) {
    print_inst(i, &p->inst[i]);
    putchar('\n');
  }
}

static Integer largest_register_number(const Program* p, int argc) {
  Integer n = (argc > 0) ? (unsigned)argc : 0;
  for (size_t i = 1; i < p->ninst; i++) {
    const ProgramInstruction* inst = p->inst + i;
    // only the first two operands are register numbers
    if (inst->argv[0] > n)
      n = inst->argv[0];
    if (inst->argv[1] > n)
      n = inst->argv[1];
  }
  return n;
}

typedef struct {
  Integer nreg;
  Integer* regs;
  Integer pc;
} Machine;

static Machine* new_machine(Integer max_user_reg) {
  Machine* m = malloc(sizeof *m);
  if (!m)
    fatal("out of memory for URM\n");
  // allocate register 0 so that register number in C = register number for user
  m->nreg = max_user_reg + 1; 
  m->regs = calloc(m->nreg, sizeof m->regs[0]);
  return m;
}

static void delete_machine(Machine* m) {
  if (m) {
    free(m->regs);
    free(m);
  }
}

static void trace(const Machine* m, const Program* p) {
  unsigned n = 0;
  if (m->pc == -1) {
    printf("%5s ", "STOP");
    n = 6;
  }
  else
    n = print_inst(m->pc, &p->inst[m->pc]);
  putchar(' ');
  for (n++; n < INST_FIELD; n++)
    putchar(' ');

  for (Integer i = 1; i < m->nreg; i++)
    printf("%5u ", m->regs[i]);
  putchar('\n');
}

static void trace_heading(Integer nreg) {
  printf("%-*s", INST_FIELD, " INST");
  for (Integer i = 1; i < nreg; i++) {
    char buf[32];
    sprintf(buf, "R%u", i);
    printf("%5s ", buf);
  }
  putchar('\n');
}

static void execute(Machine* m, const ProgramInstruction* inst) {
  // pre-emptively point to next instruction: overridden by J if applicable
  m->pc++;
  switch (inst->minst->inst) {
    case ZERO:
      assert(inst->argv[0] < m->nreg);
      m->regs[inst->argv[0]] = 0;
      break;
    case SUCC:
      assert(inst->argv[0] < m->nreg);
      m->regs[inst->argv[0]] += 1;
      break;
    case COPY:
      assert(inst->argv[0] < m->nreg);
      assert(inst->argv[1] < m->nreg);
      m->regs[inst->argv[1]] = m->regs[inst->argv[0]];
      break;
    case JUMP:
      assert(inst->argv[0] < m->nreg);
      assert(inst->argv[1] < m->nreg);
      if (m->regs[inst->argv[0]] == m->regs[inst->argv[1]])
        m->pc = inst->argv[2];
  }
}

static void run_program(Machine* m, const Program* p) {
  m->pc = 1;
  trace_heading(m->nreg);
  while (m->pc < p->ninst) {
    trace(m, p);
    execute(m, &p->inst[m->pc]);
  }
  m->pc = -1;
  trace(m, p);
}

int main(int argc, char* argv[]) {
  if (argc < 2)
    fatal("usage: URM program [r1 r2 ...]\n");

  Program* prog = load_program(argv[1]);
  //list_program(prog);

  Integer max_user_reg = largest_register_number(prog, argc - 2);
  Machine* machine = new_machine(max_user_reg);
  Integer reg = 1;
  for (int arg = 2; arg < argc; arg++)
    machine->regs[reg++] = (Integer) strtoull(argv[arg], NULL, 10);
  run_program(machine, prog);
  delete_machine(machine);

  delete_program(prog);
}
