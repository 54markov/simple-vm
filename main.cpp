#include <iostream>

#include "stack-vm.hpp"

int main(int argc, char const *argv[])
{
    auto vm = StackVM();

    vm.mem_write(
        0,
        ((static_cast<uint8_t>(Opcodes::add) & 0xf) << 12) |
        ((static_cast<uint8_t>(Register::r0) & 0x7) << 9)  |
        ((static_cast<uint8_t>(Register::r1) & 0x7) << 6)  |
         (static_cast<uint8_t>(Register::r2) & 0x7)
    );

    try
    {
        bool running = true;
        while(running)
        {
            vm.execute(vm.fetch());
            running = false;
        }
        vm.dump_registers();
    }
    catch (std::string err)
    {
        std::cerr << err << std::endl;
    }
    // Shutdown
    return 0;
}