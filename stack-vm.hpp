#pragma once

#include <unistd.h>

#include <iostream>

#include <cstdint>

#include <sys/select.h>


/*
 * The LC-3 has 10 total registers, each of which is 16 bits.
 * Most of them are general purpose, but a few have designated roles.
 */
enum class Register
{
    r0 = 0,
    r1,
    r2,
    r3,
    r4,
    r5,
    r6,
    r7,
    pc,
    cond,
    size
};

/*
 * Opcodes
 */
enum class Opcodes
{
    branch = 0,
    add,
    load,
    store,
    jump_reg,
    bit_and,
    load_reg,
    store_reg,
    rti, // unused
    bit_not,
    load_i,
    store_i,
    jump,
    reserved,
    load_eaddr,
    trap
};

/*
 * Condition flags
 */
enum class ConditionFlags
{
    positive = 1 << 0,
    zero = 1 << 1,
    negative = 1 << 2
};

/*
 * Memory Mapped Registers
 */
enum class MemoryMappedRegisters
{
    keyboard_status = 0xFE00,
    keyboard_data = 0xFE02
};

/*
 * Trap Routines
 */
enum class TrapRoutines
{
    getc = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    out = 0x21,   /* output a character */
    puts = 0x22,  /* output a word string */
    in = 0x23,    /* get character from keyboard, echoed onto the terminal */
    putsp = 0x24, /* output a byte string */
    halt = 0x25   /* halt the program */
};

class StackVM
{
    private:
        /*
        * The LC-3 has 65,536 memory locations.
        * The maximum that is addressable by a 16-bit unsigned integer
        */
        uint16_t memory_[UINT16_MAX] = { 0 };
        uint16_t registers_[static_cast<uint16_t>(Register::size)] = { 0 };

        void add_(uint16_t instr);
        void bit_and_(const uint16_t instr);
        void bit_not_(const uint16_t instr);
        void branch_(const uint16_t instr);
        void jump_(const uint16_t instr);
        void jump_reg_(const uint16_t instr);
        void load_(const uint16_t instr);
        void load_reg_(const uint16_t instr);
        void load_eaddr_(const uint16_t instr);
        void store_(const uint16_t instr);
        void store_i(const uint16_t instr);
        void store_reg(const uint16_t instr);
        void trap_(const uint16_t instr);

        const uint16_t check_key_();
        void update_flags_(const::Register r);
        const uint16_t sign_extend_(const uint16_t x, const int bit_count);

    public:
        StackVM(/* args */);
        ~StackVM() = default;

        void load_image(/*ARGS*/);

        const uint16_t fetch();
        const uint16_t execute(const uint16_t instr);

        void mem_write(const uint16_t address, const uint16_t value);
        const uint16_t mem_read(const uint16_t address);

        void dump_memory();
        void dump_registers();
        void dump_register(const Register reg);
};

// unit tests
// assembly code generating