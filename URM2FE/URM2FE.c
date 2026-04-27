// URM2FE: URM to front end
// Rewrite URM program in frontend language for easier comprehension.
// Nigel Perks, 2026.

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

typedef unsigned long Integer;

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
  Integer* labels;
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
  p->labels = NULL;
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

static Program* load_program_file(FILE* fp) {
  Program* p = new_program();
  char line[128];
  unsigned lineno = 1;
  while (read_line(lineno, line, sizeof line, fp)) {
    ensure_space(p);
    parse_line(&p->inst[p->ninst], lineno, line);
    p->ninst++;
    lineno++;
  }
  return p;
}

static Program* load_program_name(const char* fname) {
  FILE* fp = fopen(fname, "r");
  if (fp == NULL)
    fatal("cannot open program file: %s\n", fname);
  Program* p = load_program_file(fp);
  fclose(fp);
  return p;
}

static void print_inst(unsigned instno, const ProgramInstruction* inst) {
  printf("%5u %c(%llu", instno, inst->minst->mnemonic, (unsigned long long) inst->argv[0]);
  for (unsigned i = 1; i < inst->minst->argc; i++)
    printf(",%llu", (unsigned long long) inst->argv[i]);
  fputs(")", stdout);
}

static void list_program(const Program* p) {
  for (unsigned i = 1; i < p->ninst; i++) {
    print_inst(i, &p->inst[i]);
    putchar('\n');
  }
}

static Integer largest_register_number(const Program* p) {
  Integer n = 0;
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

static void analyse_jumps(Program* prog) {
  if (prog->inst == NULL || prog->ninst == 0)
    return;
  prog->labels = calloc(prog->ninst, sizeof prog->labels[0]);

  // flag which lines need to be labelled
  for (unsigned i = 1; i < prog->ninst; i++) {
    if (prog->inst[i].minst->inst == JUMP) {
      Integer dest = prog->inst[i].argv[2];
      if (dest < prog->ninst)
        prog->labels[dest] = 1;
    }
  }

  // assign label numbers in line number order
  Integer next_label = 1;
  for (unsigned i = 1; i < prog->ninst; i++) {
    if (prog->labels[i])
      prog->labels[i] = next_label++;
  }
}

static void print_reg_name(Integer reg, unsigned namec, const char* namev[]) {
  if (reg < namec)
    fputs(namev[reg], stdout);
  else
    printf("r%lu", reg);
}

static void print_line_label(const Program* prog, Integer line) {
  assert(line > 0 && line < prog->ninst);
  printf("L%lu", prog->labels[line]);
}

static void print_frontend_inst(const Program* prog, unsigned i, bool assignments, unsigned namec, const char* namev[]) {
  assert(i < prog->ninst);
  assert(sizeof (Integer) == sizeof (unsigned long));

  if (prog->labels[i])
    printf("L%lu:\n", prog->labels[i]);

  fputs("  ", stdout);
  const ProgramInstruction* inst = &prog->inst[i];
  switch (inst->minst->inst) {
    case NOOP:
      break;
    case ZERO:
      if (assignments) {
        print_reg_name(inst->argv[0], namec, namev);
        puts(" := 0");
      }
      else {
        fputs("zero ", stdout);
        print_reg_name(inst->argv[0], namec, namev);
        putchar('\n');
      }
      break;
    case SUCC:
      if (assignments) {
        print_reg_name(inst->argv[0], namec, namev);
        fputs(" := ", stdout);
        print_reg_name(inst->argv[0], namec, namev);
        puts(" + 1");
      }
      else {
        fputs("inc ", stdout);
        print_reg_name(inst->argv[0], namec, namev);
        putchar('\n');
      }
      break;
    case COPY:
      if (assignments) {
        print_reg_name(inst->argv[1], namec, namev);
        fputs(" := ", stdout);
        print_reg_name(inst->argv[0], namec, namev);
        putchar('\n');
      }
      else {
        fputs("copy ", stdout);
        print_reg_name(inst->argv[0], namec, namev);
        fputs(" to ", stdout);
        print_reg_name(inst->argv[1], namec, namev);
        putchar('\n');
      }
      break;
    case JUMP:
      if (inst->argv[0] != inst->argv[1]) {
        fputs("if ", stdout);
        print_reg_name(inst->argv[0], namec, namev);
        fputs(" = ", stdout);
        print_reg_name(inst->argv[1], namec, namev);
        fputs(" then ", stdout);
      }
      if (inst->argv[2] >= prog->ninst)
        puts("goto _stop");
      else        
        printf("goto L%lu\n", prog->labels[inst->argv[2]]);
      break;
    default:
      fatal("unknown URM instruction: %u\n", inst->minst->inst);
  }
}

static void declare_names(Integer inputs, Integer maxreg, unsigned namec, const char* namev[]) {
  if (inputs) {
    puts("; input registers");
    fputs("  declare ", stdout);
    print_reg_name(1, namec, namev);
    for (Integer reg = 2; reg <= inputs; reg++) {
      fputs(", ", stdout);
      print_reg_name(reg, namec, namev);
    }
    putchar('\n');
  }

  if (maxreg > inputs) {
    puts("; other registers used");
    fputs("  declare ", stdout);
    print_reg_name(inputs + 1, namec, namev);
    for (Integer reg = inputs + 2; reg <= maxreg; reg++) {
      fputs(", ", stdout);
      print_reg_name(reg, namec, namev);
    }
    putchar('\n');
  }

  if (inputs || maxreg > inputs)
    putchar('\n');
}

static void decompile(const Program* prog, bool assignments, unsigned namec, const char* namev[], unsigned inputs) {
  Integer maxreg = largest_register_number(prog);
  declare_names(inputs, maxreg, namec, namev);

  for (unsigned i = 1; i < prog->ninst; i++)
    print_frontend_inst(prog, i, assignments, namec, namev);
  puts("_stop:");
}

#define MAX_NAMES (16)

int main(int argc, char* argv[]) {
  const char* fname = NULL;
  unsigned inputs = 0;
  bool assignments = false;
  const char* namev[MAX_NAMES];
  unsigned namec = 0;

  namev[namec++] = "R0";

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (argv[i][1] == 'a')
        assignments = true;
      else
        inputs = strtoul(argv[i]+1, NULL, 10);
    }
    else if (fname == NULL)
      fname = argv[i];
    else {
      if (namec >= MAX_NAMES)
        fatal("too many names: %s\n", argv[i]);
      namev[namec++] = argv[i];
    }
  }

  Program* prog = fname ? load_program_name(fname) : load_program_file(stdin);
  //list_program(prog);
  analyse_jumps(prog);
  decompile(prog, assignments, namec, namev, inputs);
  delete_program(prog);
}
