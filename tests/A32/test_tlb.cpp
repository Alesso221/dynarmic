/* This file is part of the dynarmic project.
 * Copyright (c) 2020 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <catch.hpp>

#include <dynarmic/A32/a32.h>

#include "common/common_types.h"
#include "testenv.h"

TEST_CASE("tlb: All entries hit", "[tlb]") {
    ArmTestEnv env;

    std::uint32_t page1[2] = { 100, 200 };
    std::uint32_t page2[3] = { 400, 600, 800 }; 

    const std::uint32_t perm = Dynarmic::MemoryPermissionRead | Dynarmic::MemoryPermissionWrite;

    Dynarmic::TLB<9> tlb(12);

    // Add entries that continuous so that they don't hit
    tlb.Add(0x12345000, reinterpret_cast<std::uint8_t*>(page1), perm);
    tlb.Add(0x12346000, reinterpret_cast<std::uint8_t*>(page2), perm);

    Dynarmic::A32::UserConfig conf{&env};
    conf.tlb_entries = tlb.entries;
    
    Dynarmic::A32::Jit jit{conf};

    env.code_mem = {
        0xe5933000,  // ldr r3, [r3]
        0xe5845000,  // str r5, [r4]
        0xeafffffe,  // b +#0
    };
    
    jit.Regs()[3] = 0x12345004;
    jit.Regs()[4] = 0x12346008;
    jit.Regs()[5] = 0x11111111;
    jit.SetCpsr(0x000001d0); // User-mode

    env.ticks_left = 3;
    jit.Run();

    REQUIRE(jit.Regs()[3] == 200);
    REQUIRE(page2[2] == 0x11111111);
}

TEST_CASE("tlb: Miss TLB", "[tlb]") {
    ArmTestEnv env;

    std::uint32_t page1[2] = { 100, 200 };

    const std::uint32_t perm = Dynarmic::MemoryPermissionRead | Dynarmic::MemoryPermissionWrite;

    Dynarmic::TLB<9> tlb(12);
    tlb.Add(0x12346000, reinterpret_cast<std::uint8_t*>(page1), perm);

    Dynarmic::A32::UserConfig conf{&env};
    conf.tlb_entries = tlb.entries;
    
    Dynarmic::A32::Jit jit{conf};

    // This time the load will miss, which trigger MemoryRead*
    env.code_mem = {
        0xe5933008,  // ldr r3, [r3, #8]
        0xe5845000,  // str r5, [r4]
        0xeafffffe,  // b +#0
    };
    
    jit.Regs()[3] = 0x12345004;
    jit.Regs()[4] = 0x12346008;
    jit.Regs()[5] = 0x11111111;
    jit.SetCpsr(0x000001d0); // User-mode

    env.ticks_left = 3;
    jit.Run();

    REQUIRE(jit.Regs()[3] == 0xf0e0d0c);
    REQUIRE(page1[2] == 0x11111111);
}

TEST_CASE("tlb: Wrong permission", "[tlb]") {
    ArmTestEnv env;

    std::uint32_t page1[2] = { 100, 200 };
    std::uint32_t page2[2] = { 300, 400 };

    const std::uint32_t perm1 = Dynarmic::MemoryPermissionWrite;
    const std::uint32_t perm2 = Dynarmic::MemoryPermissionRead | Dynarmic::MemoryPermissionWrite;

    Dynarmic::TLB<9> tlb(12);
    tlb.Add(0x12345000, reinterpret_cast<std::uint8_t*>(page1), perm1);
    tlb.Add(0x12346000, reinterpret_cast<std::uint8_t*>(page2), perm2);

    Dynarmic::A32::UserConfig conf{&env};
    conf.tlb_entries = tlb.entries;
    
    Dynarmic::A32::Jit jit{conf};

    // This time the load will fail (permission only allow write), which trigger MemoryRead*
    // In real usage no one will forbid read though ;)
    env.code_mem = {
        0xe5933008,  // ldr r3, [r3, #8]
        0xe5944004,  // ldr r4, [r4, #4]
        0xeafffffe,  // b +#0
    };
    
    jit.Regs()[3] = 0x12345004;
    jit.Regs()[4] = 0x12346000;
    jit.SetCpsr(0x000001d0); // User-mode

    env.ticks_left = 3;
    jit.Run();

    // The first instruction will miss TLB, and fallback to MemoryRead*
    REQUIRE(jit.Regs()[3] == 0xf0e0d0c);
    REQUIRE(jit.Regs()[4] == 400);
}