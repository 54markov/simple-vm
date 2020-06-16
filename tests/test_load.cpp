#include "../stack-vm.hpp"

#include <iostream>


int main()
{
    auto vm = StackVM();

    uint16_t load_instr = ((static_cast<uint16_t>(Opcodes::load) & 0xf) << 12) |
                          ((static_cast<uint16_t>(Register::r0) & 0x7) << 9)   |
                          0x0;

    vm.mem_write(0, load_instr);
    vm.mem_write(1, 0x2);

    vm.execute(vm.fetch());
    vm.dump_register(Register::r0);

    return 0;
}
