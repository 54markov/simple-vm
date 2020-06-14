#include "stack-vm.hpp"

#include <string>


StackVM::StackVM()
{
    this->registers_[1] = 0x5;
    this->registers_[2] = 0x6;
    ;
}

void StackVM::load_image(/*ARGS*/)
{
    return; // TODO
}

const uint16_t StackVM::fetch()
{
    const auto pc = this->registers_[static_cast<uint16_t>(Register::pc)]++;
    if (pc > UINT16_MAX)
    {
        throw std::string("overflow program counter");
    }

    return this->mem_read(pc);
}

const uint16_t StackVM::execute(const uint16_t instr)
{
    uint16_t opcode = instr >> 12;
    switch (Opcodes(opcode))
    {
        case Opcodes::add:
            this->add_(instr);
            break;
        default:
            throw std::string("bad opcode");
            break;
    }
    return 0; // TODO
}

void StackVM::mem_write(const uint16_t address, const uint16_t value)
{
    if (address > UINT16_MAX)
    {
        throw std::string("write: invalid memory address");
    }

    this->memory_[address] = value;
}

const uint16_t StackVM::mem_read(const uint16_t address)
{
    if (address > UINT16_MAX)
    {
        throw std::string("read: invalid memory address");
    }

    if (address == MemoryMappedRegisters::keyboard_status)
    {
        if (this->check_key_())
        {
            this->memory_[MemoryMappedRegisters::keyboard_status] = (1 << 15);
            this->memory_[MemoryMappedRegisters::keyboard_data] = ::getchar();
        }
        else
        {
            this->memory_[MemoryMappedRegisters::keyboard_status] = 0;
        }
    }

    return this->memory_[address];
}


void StackVM::dump_memory()
{
    for (auto i = 0; i < UINT16_MAX; ++i)
    {
        std::cout << "memory[" << i << "] : " << this->memory_[i] << std::endl;
    }
}

void StackVM::dump_registers()
{
    for (uint16_t r = 0; r < static_cast<uint16_t>(Register::size); r++)
    {
        this->dump_register(static_cast<Register>(r));
    }
}

void StackVM::dump_register(const Register reg)
{
    std::string reg_str;
    switch (reg)
    {
        case Register::r0:
            reg_str = "R0  ";
            break;
        case Register::r1:
            reg_str = "R1  ";
            break;
        case Register::r2:
            reg_str = "R2  ";
            break;
        case Register::r3:
            reg_str = "R3  ";
            break;
        case Register::r4:
            reg_str = "R4  ";
            break;
        case Register::r5:
            reg_str = "R5  ";
            break;
        case Register::r6:
            reg_str = "R6  ";
            break;
        case Register::r7:
            reg_str = "R7  ";
            break;
        case Register::pc:
            reg_str = "PC  ";
            break;
        case Register::cond:
            reg_str = "COND";
            break;
        default:
            std::cerr << "unkown register " << static_cast<uint16_t>(reg) << std::endl;
            break;
    }
    std::cout << reg_str << ": 0x" << this->registers_[static_cast<uint16_t>(reg)] << std::endl;
}

void StackVM::add_(const uint16_t instr)
{
    /*
     * 15   12 11 9 8   6  5  4  3 2   0
     * +------+----+-----+---+----+-----+
     * | 0001 | DR | SR1 | 0 | 00 | SR2 |
     * +------+----+-----+---+----+-----+
     *
     * ADD R2 R0 R1 ; add the contents of R0 to R1 and store in R2.
     *
     * 15   12 11 9 8   6  5  4        0
     * +------+----+-----+---+----------+
     * | 0001 | DR | SR1 | 1 |   imm5   |
     * +------+----+-----+---+----------+
     *
     * ADD R0 R0 1 ; add 1 to R0 and store back in R0
     *
     */

    uint16_t r0 = (instr >> 9) & 0x7;       // Destination register (DR)
    uint16_t r1 = (instr >> 6) & 0x7;       // First operand (SR1)
    uint16_t imm_flag = (instr >> 5) & 0x1; // Whether we are in immediate mode

    if (imm_flag)
    {
        uint16_t imm5 = this->sign_extend_(instr & 0x1F, 5);
        this->registers_[r0] = this->registers_[r1] + imm5;
    }
    else
    {
        uint16_t r2 = instr & 0x7;
        this->registers_[r0] = this->registers_[r1] + this->registers_[r2];
    }

    this->update_flags_(static_cast<Register>(r0));
}

const uint16_t StackVM::check_key_()
{
    fd_set readfds = {};
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 0,
    };
    return ::select(1, &readfds, nullptr, nullptr, &timeout) != 0;
}

void StackVM::update_flags_(const::Register r)
{
    if (this->registers_[static_cast<uint16_t>(r)] == 0)
    {
        this->registers_[static_cast<uint16_t>(Register::cond)] = ConditionFlags::zero;
    }
    else if (this->registers_[static_cast<uint16_t>(r)] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        this->registers_[static_cast<uint16_t>(Register::cond)] = ConditionFlags::negative;
    }
    else
    {
        this->registers_[static_cast<uint16_t>(Register::cond)] = ConditionFlags::positive;
    }
}

const uint16_t StackVM::sign_extend_(const uint16_t x, const int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        return x | (0xFFFF << bit_count);
    }
    return x;
}