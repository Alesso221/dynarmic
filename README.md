Dynarmic
========

[![Travis CI Build Status](https://api.travis-ci.org/MerryMage/dynarmic.svg?branch=master)](https://travis-ci.org/MerryMage/dynarmic/branches) [![Appveyor CI Build status](https://ci.appveyor.com/api/projects/status/maeiqr41rgm1innm/branch/master?svg=true)](https://ci.appveyor.com/project/MerryMage/dynarmic/branch/master)

A dynamic recompiler for ARM.

### Supported guest architectures

* ARMv6K
* 64-bit ARMv8

### Supported host architectures

* x86-64

There are no plans to support x86-32.

Documentation
-------------

Design documentation can be found at [docs/Design.md](docs/Design.md).

Plans
-----

### Near-term

* Complete ARMv8 support

### Medium-term

* Optimizations

### Long-term

* ARMv7A guest support
* ARMv5 guest support
* ARMv8 host support

Usage Example
-------------

The below is a minimal example. Bring-your-own memory system.

```cpp
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>

#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/config.h>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

class MyEnvironment final : public Dynarmic::A32::UserCallbacks {
public:
    u64 ticks_left = 0;
    std::array<u8, 2048> memory{};

    u8 MemoryRead8(u32 vaddr) override {
        if (vaddr >= memory.size()) {
            return 0;
        }
        return memory[vaddr];
    }

    u16 MemoryRead16(u32 vaddr) override {
        return u16(MemoryRead8(vaddr)) | u16(MemoryRead8(vaddr + 1)) << 8;
    }

    u32 MemoryRead32(u32 vaddr) override {
        return u32(MemoryRead16(vaddr)) | u32(MemoryRead16(vaddr + 2)) << 16;
    }

    u64 MemoryRead64(u32 vaddr) override {
        return u64(MemoryRead32(vaddr)) | u64(MemoryRead32(vaddr + 4)) << 32;
    }

    void MemoryWrite8(u32 vaddr, u8 value) override {
        if (vaddr >= memory.size()) {
            return;
        }
        memory[vaddr] = value;
    }

    void MemoryWrite16(u32 vaddr, u16 value) override {
        MemoryWrite8(vaddr, u8(value));
        MemoryWrite8(vaddr + 1, u8(value >> 8));
    }

    void MemoryWrite32(u32 vaddr, u32 value) override {
        MemoryWrite16(vaddr, u16(value));
        MemoryWrite16(vaddr + 2, u16(value >> 16));
    }

    void MemoryWrite64(u32 vaddr, u64 value) override {
        MemoryWrite32(vaddr, u32(value));
        MemoryWrite32(vaddr + 4, u32(value >> 32));
    }

    void InterpreterFallback(u32 pc, size_t num_instructions) override {
        // This is never called in practice.
        std::terminate();
    }

    void CallSVC(u32 swi) override {
        // Do something.
    }

    void ExceptionRaised(u32 pc, Dynarmic::A32::Exception exception) override {
        // Do something.
    }

    void AddTicks(u64 ticks) override {
        if (ticks > ticks_left) {
            ticks_left = 0;
            return;
        }
        ticks_left -= ticks;
    }

    u64 GetTicksRemaining() override {
        return ticks_left;
    }
};

int main(int argc, char** argv) {
    MyEnvironment env;
    Dynarmic::A32::UserConfig user_config;
    user_config.callbacks = &env;
    Dynarmic::A32::Jit cpu{user_config};

    // Execute at least 1 instruction.
    // (Note: More than one instruction may be executed.)
    env.ticks_left = 1;

    // Write some code to memory.
    env.MemoryWrite16(0, 0x0088); // lsls r0, r1, #2
    env.MemoryWrite16(2, 0xE7FE); // b +#0 (infinite loop)

    // Setup registers.
    cpu.Regs()[0] = 1;
    cpu.Regs()[1] = 2;
    cpu.Regs()[15] = 0; // PC = 0
    cpu.SetCpsr(0x00000030); // Thumb mode

    // Execute!
    cpu.Run();

    // Here we would expect jit.Regs()[0] == 8
    printf("R0: %u\n", jit.Regs()[0]);

    return 0;
}
```
