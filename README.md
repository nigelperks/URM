# UNLIMITED REGISTER MACHINE (URM) SIMULATOR

The Unlimited Register Machine (URM)
is a model of computation, like the Turing Machine.
This is a simulator of the version used in
Nigel Cutland's book "Computability"
and Open University course M381 Number Theory and Mathematical Logic.

If you use the source, please preserve the original author name and date.

## URM

A URM consists of an arbitrary number of registers
able to hold non-negative integers of unlimited size.
The registers are referred to as R1, R2, R3, ... or just by their number.
The values in the registers can be denoted r1, r2, r3, ... .

The URM has four instructions:

- `Z(i)` - zero - store zero in register `i`
- `S(i)` - successor - increment register `i`
- `C(i,j)` - copy - copy register `i` to register `j`
- `J(i,j,p)` - jump - if the value in register `i` equals the value in register `j`, jump to instruction (line) `p`, or stop if `p` exceeds the final line number

See `URM/EXAMPLES`.

The program in `URM.c` simulates the URM.

If the C program is compiled to `URM.exe`, then:

    URM.exe add.urm 6 7

runs the URM program `add.urm` in the URM,
initialising register 1 to 6, register 2 to 7,
and any other registers used to zero.

It outputs a trace of the running program,
each line showing the state of the registers _before_ an instruction is executed.

By convention, the value of R1 at termination is the output value.

## URMFE

The program in `URMFE.c` is a URM **front end**,
a translator from a simple language into URM code.

This language is my own, not from a book or course.

The frontend allows variable names to be used instead of register numbers,
and labels instead of instruction numbers.

When a variable name is encountered for the first time,
it is allocated the next available register.

The `declare` statement declares names for the next available registers.
This is useful to name the inputs in R1, R2, ... .

The basic instructions correspond to those of the URM:

| URM | Frontend | Alternative |
| ----------- | ----------- | ----------- |
| `Z(i)` | `zero foo` | `foo := 0` |
| `S(i)` | `inc foo` | `foo := foo + 1` |
| `C(i,j)` | `copy foo to bar` | `bar := foo` |
| `J(i,j,p)` | `if foo = bar then goto label` | |

For example, the addition program:

    1 J(2,3,5)
    2 S(1)
    3 S(3)
    4 J(1,1,1)

can be expressed:

    declare n, m
    test:
    if count = m then goto stop
    inc n
    inc count
    goto test
    stop:

There are some slightly higher level convenience features:

- assigning, and incrementing by, any non-negative integer
- a structured `if` statement
- a `while` loop
- a `for` loop handling initialisation, comparison test, and increment

See the comments at the head of `URMFE.c`, and the examples.

Note that the `for` loop limit is the value for terminating the loop,
not the final value for which the loop body is executed.

This language is meant to be a more convenient notation and abbreviation
for URM operations, just one step up from the URM code.

Nigel Perks, 2026
