/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <cstdint>
#include <vector>

namespace Dynarmic {

using VAddr = std::uint64_t;

enum MemoryPermission {
    MemoryPermissionRead = 1 << 0,
    MemoryPermissionWrite = 1 << 1,
    MemoryPermissionExecute = 1 << 2
};

struct TLBEntry {
    VAddr read_addr;
    VAddr write_addr;
    VAddr execute_addr;

    std::uint8_t *host_base;

    explicit TLBEntry()
        : read_addr(0)
        , write_addr(0)
        , execute_addr(0)
        , host_base(nullptr) {
    }
};

template <std::uint32_t TLB_BIT_COUNT>
struct TLB {
public:
    static constexpr std::uint32_t TLB_ENTRY_COUNT = 1 << TLB_BIT_COUNT;
    static constexpr std::uint32_t TLB_BIT_MASK = (1 << TLB_BIT_COUNT) - 1;

    TLBEntry entries[TLB_ENTRY_COUNT];

    std::size_t page_bits;
    std::size_t page_mask;

    explicit TLB(std::size_t page_bits)
        : page_bits(page_bits) {
        page_mask = (1 << page_bits) - 1;

        /*
        for (std::size_t i = 0; i < TLB_ENTRY_COUNT; i++) {
            entries[i].read_addr = i + 1;
            entries[i].write_addr = i + 1;
        }*/
        Flush();
    }

    void Flush() {
        // Using memfill to speed up this process
        std::memset(entries, 0, sizeof(TLBEntry) * TLB_ENTRY_COUNT);
    }

    void Add(VAddr addr, std::uint8_t *host, const std::uint32_t perm) {
        const std::size_t page_index = addr >> page_bits;
        const std::size_t tlb_index = page_index & (TLB_ENTRY_COUNT - 1);
        const std::size_t addr_mod = addr & page_mask;
        const VAddr addr_normed = addr & ~page_mask;

        TLBEntry &entry = entries[tlb_index];
        entry.host_base = host - addr_mod;

        if (perm & MemoryPermissionRead) {
            entry.read_addr = addr_normed;
        } else {
            entry.read_addr = 0;
        }

        if (perm & MemoryPermissionWrite) {
            entry.write_addr = addr_normed;
        } else {
            entry.write_addr = 0;
        }
        
        if (perm & MemoryPermissionExecute) {
            entry.execute_addr = addr_normed;
        } else {
            entry.execute_addr = 0;
        }
    }

    void MakeDirty(VAddr addr) {
        const std::size_t page_index = addr >> page_bits;
        const std::size_t tlb_index = page_index & (TLB_ENTRY_COUNT - 1);
        const VAddr addr_normed = addr & ~page_mask;

        TLBEntry &entry = entries[tlb_index];

        if ((entry.read_addr == addr_normed) || (entry.write_addr == addr_normed) ||
            (entry.execute_addr == addr_normed)) {
            std::memset(&entry, 0, sizeof(TLBEntry));
        }
    }

    std::uint8_t *Lookup(VAddr addr) {
        const std::size_t page_index = addr >> page_bits;
        const std::size_t tlb_index = page_index & (TLB_ENTRY_COUNT - 1);
        const VAddr addr_normed = addr & ~page_mask;

        TLBEntry &entry = entries[tlb_index];

        if (!entry.host_base) {
            return nullptr;
        }
        
        if ((entry.read_addr == addr_normed) || (entry.write_addr == addr_normed) ||
            (entry.execute_addr == addr_normed)) {
            const std::size_t addr_mod = addr & page_mask;
            return entry.host_base + addr_mod;
        }

        // TLB miss
        return nullptr;
    }
};

}