#include "stack-vm.hpp"

#include <string>

static uint16_t swap16(uint16_t x)
{
    return (x << 8) | (x >> 8);
}

void StackVM::load_image(const char* path)
{
    // TODO: c++ binary reading

    FILE* file = ::fopen(path, "rb");
    if (!file)
    {
        throw std::string("can't open file");
    }

    // The origin tells us where in memory to place the image
    uint16_t origin = 0;
    if (::fread(&origin, sizeof(origin), 1, file) != 1)
    {
        ::fclose(file);
        throw std::string("can't read file");
    }
    origin = swap16(origin);

    // We know the maximum file size so we only need one fread
    uint16_t max_read = UINT16_MAX - origin;
    uint16_t* p = this->memory_ + origin;
    size_t read = ::fread(p, sizeof(uint16_t), max_read, file);

    // Swap to little endian
    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }

    ::fclose(file);
}

const bool StackVM::is_running()
{
    return this->running_;
}

const uint16_t StackVM::fetch()
{
    const auto pc = this->registers_[static_cast<uint16_t>(Register::pc)]++;
    return this->mem_read(pc);
}

const void StackVM::execute(const uint16_t instr)
{
    switch (Opcodes(instr >> 12))
    {
        case Opcodes::branch:
            this->branch_(instr);
            break;
        case Opcodes::add:
            this->add_(instr);
            break;
        case Opcodes::load:
            this->load_(instr);
            break;
        case Opcodes::store:
            this->store_(instr);
            break;
        case Opcodes::jump_reg:
            this->jump_reg_(instr);
            break;
        case Opcodes::bit_and:
            this->bit_and_(instr);
            break;
        case Opcodes::load_reg:
            this->load_reg_(instr);
            break;
        case Opcodes::store_reg:
            this->store_reg_(instr);
            break;
        case Opcodes::rti: // unused
            break;
        case Opcodes::bit_not:
            this->bit_not_(instr);
            break;
        case Opcodes::load_i:
            this->load_i_(instr);
            break;
        case Opcodes::store_i:
            this->store_i_(instr);
            break;
        case Opcodes::jump:
            this->jump_(instr);
            break;
        case Opcodes::reserved:
            break;
        case Opcodes::load_eaddr:
            this->load_eaddr_(instr);
            break;
        case Opcodes::trap:
            this->trap_(instr);
            break;
        default:
            throw std::string("bad opcode");
            break;
    }
}

void StackVM::mem_write(const uint16_t address, const uint16_t value)
{
    this->memory_[address] = value;
}

const uint16_t StackVM::mem_read(const uint16_t address)
{
    if (address == static_cast<uint16_t>(MemoryMappedRegisters::keyboard_status))
    {
        if (this->check_key_())
        {
            this->memory_[static_cast<uint16_t>(MemoryMappedRegisters::keyboard_status)] = (1 << 15);
            this->memory_[static_cast<uint16_t>(MemoryMappedRegisters::keyboard_data)] = ::getchar();
        }
        else
        {
            this->memory_[static_cast<uint16_t>(MemoryMappedRegisters::keyboard_status)] = 0;
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
            std::cerr << "unknown register " << static_cast<uint16_t>(reg) << std::endl;
            break;
    }
    std::cout << reg_str << ": 0x" << this->registers_[static_cast<uint16_t>(reg)] << std::endl;
}

void StackVM::branch_(const uint16_t instr)
{
    uint16_t pc_offset = this->sign_extend_((instr) & 0x1ff, 9);
    uint16_t cond_flag = (instr >> 9) & 0x7;

    if (cond_flag & this->registers_[static_cast<uint16_t>(Register::cond)])
    {
        this->registers_[static_cast<uint16_t>(Register::pc)] += pc_offset;
    }
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

void StackVM::load_(const uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = this->sign_extend_(instr & 0x1FF, 9);

    this->registers_[r0] = this->mem_read(this->registers_[static_cast<uint16_t>(Register::pc)] + pc_offset);
    this->update_flags_(static_cast<Register>(r0));
}

void StackVM::store_(const uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = this->sign_extend_(instr & 0x1FF, 9);

    this->mem_write(this->registers_[static_cast<uint16_t>(Register::pc)] + pc_offset, this->registers_[r0]);
}

void StackVM::jump_reg_(const uint16_t instr)
{
    uint16_t long_flag = (instr >> 11) & 1;
    this->registers_[static_cast<uint16_t>(Register::r7)] = this->registers_[static_cast<uint16_t>(Register::pc)];

    if (long_flag)
    {
        uint16_t long_pc_offset = this->sign_extend_(instr & 0x7FF, 11);
        this->registers_[static_cast<uint16_t>(Register::pc)] += long_pc_offset; // JSR
    }
    else
    {
        uint16_t r1 = (instr >> 6) & 0x7;
        this->registers_[static_cast<uint16_t>(Register::pc)] = this->registers_[r1]; // JSRR
    }
}

void StackVM::bit_and_(const uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if (imm_flag)
    {
        uint16_t imm5 = this->sign_extend_(instr & 0x1F, 5);
        this->registers_[r0] = this->registers_[r1] & imm5;
    }
    else
    {
        uint16_t r2 = instr & 0x7;
        this->registers_[r0] = this->registers_[r1] & this->registers_[r2];
    }

    this->update_flags_(static_cast<Register>(r0));
}

void StackVM::load_reg_(const uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;

    uint16_t offset = this->sign_extend_(instr & 0x3F, 6);
    this->registers_[r0] = this->mem_read(this->registers_[r1] + offset);

    this->update_flags_(static_cast<Register>(r0));
}

void StackVM::store_reg_(const uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;

    uint16_t offset = this->sign_extend_(instr & 0x3F, 6);
    this->mem_write(this->registers_[r1] + offset, this->registers_[r0]);
}

void StackVM::bit_not_(const uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;

    this->registers_[r0] = ~this->registers_[r1];
    this->update_flags_(static_cast<Register>(r0));
}

void StackVM::load_i_(const uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;                          // Destination register (DR)
    uint16_t pc_offset = this->sign_extend_(instr & 0x1FF, 9); // PCoffset 9
    // Add pc_offset to the current PC, look at that memory location to get the final address
    this->registers_[r0] = this->mem_read(this->mem_read(this->registers_[static_cast<uint16_t>(Register::pc)] + pc_offset));

    this->update_flags_(static_cast<Register>(r0));
}

void StackVM::store_i_(const uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = this->sign_extend_(instr & 0x1FF, 9);

    this->mem_write(this->mem_read(this->registers_[static_cast<uint16_t>(Register::pc)] + pc_offset), this->registers_[r0]);
}

void StackVM::jump_(const uint16_t instr)
{
    // Also handles RET
    uint16_t r1 = (instr >> 6) & 0x7;
    this->registers_[static_cast<uint16_t>(Register::pc)] = this->registers_[r1];
}

void StackVM::load_eaddr_(const uint16_t instr)
{
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = this->sign_extend_(instr & 0x1FF, 9);
    this->registers_[r0] = this->registers_[static_cast<uint16_t>(Register::pc)] + pc_offset;

    this->update_flags_(static_cast<Register>(r0));
}

void StackVM::trap_(const uint16_t instr)
{
    switch (TrapRoutines(instr & 0xFF))
    {
        case TrapRoutines::getc:
        {
            // Read a single ASCII char
            this->registers_[static_cast<uint16_t>(Register::r0)] = (uint16_t)::getchar();
            break;
        }
        case TrapRoutines::out:
        {
            ::putc((char)this->registers_[static_cast<uint16_t>(Register::r0)], stdout);
            ::fflush(stdout);
            break;
        }
        case TrapRoutines::puts:
        {
            // One char per word
            uint16_t* c = this->memory_ + this->registers_[static_cast<uint16_t>(Register::r0)];
            while (*c)
            {
                ::putc((char)*c, stdout);
                ++c;
            }
            ::fflush(stdout);
            break;
        }
        case TrapRoutines::in:
        {
            std::cout << "Enter a character: ";
            this->registers_[static_cast<uint16_t>(Register::r0)] = (uint16_t)getchar();
            break;
        }
        case TrapRoutines::putsp:
        {
            // One char per byte (two bytes per word) here we need to swap back to big endian format
            uint16_t* c = this->memory_ + this->registers_[static_cast<uint16_t>(Register::r0)];
            while (*c)
            {
                char char1 = (*c) & 0xFF;
                ::putc(char1, stdout);
                char char2 = (*c) >> 8;
                if (char2)
                {
                    ::putc(char2, stdout);
                }
                ++c;
            }
            ::fflush(stdout);
            break;
        }
        case TrapRoutines::halt:
            std::cout << "Halt" << std::endl;
            this->running_ = false;
            break;
        }
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
        this->registers_[static_cast<uint16_t>(Register::cond)] = static_cast<uint16_t>(ConditionFlags::zero);
    }
    else if (this->registers_[static_cast<uint16_t>(r)] >> 15) // a 1 in the left-most bit indicates negative
    {
        this->registers_[static_cast<uint16_t>(Register::cond)] = static_cast<uint16_t>(ConditionFlags::negative);
    }
    else
    {
        this->registers_[static_cast<uint16_t>(Register::cond)] = static_cast<uint16_t>(ConditionFlags::positive);
    }
}

const uint16_t StackVM::sign_extend_(const uint16_t x, const int bit_count)
{
    if ((x >> (bit_count - 1)) & 1) {
        return x | (0xFFFF << bit_count);
    }

    return x;
}
