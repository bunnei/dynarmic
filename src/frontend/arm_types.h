/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

#pragma once

#include <string>
#include <tuple>
#include <utility>

#include "common/assert.h"
#include "common/common_types.h"
#include "frontend/arm/FPSCR.h"

namespace Dynarmic {
namespace Arm {

enum class Cond {
    EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV
};

enum class Reg {
    R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15,
    SP = R13,
    LR = R14,
    PC = R15,
    INVALID_REG = 99
};

enum class ExtReg {
    S0, S1, S2, S3, S4, S5, S6, S7,
    S8, S9, S10, S11, S12, S13, S14, S15,
    S16, S17, S18, S19, S20, S21, S22, S23,
    S24, S25, S26, S27, S28, S29, S30, S31,
    D0, D1, D2, D3, D4, D5, D6, D7,
    D8, D9, D10, D11, D12, D13, D14, D15,
    D16, D17, D18, D19, D20, D21, D22, D23,
    D24, D25, D26, D27, D28, D29, D30, D31,
};

using Imm3 = u32;
using Imm4 = u32;
using Imm5 = u32;
using Imm7 = u32;
using Imm8 = u32;
using Imm11 = u32;
using Imm12 = u32;
using Imm24 = u32;
using RegList = u16;

enum class ShiftType {
    LSL,
    LSR,
    ASR,
    ROR ///< RRX falls under this too
};

enum class SignExtendRotation {
    ROR_0,  ///< ROR #0 or omitted
    ROR_8,  ///< ROR #8
    ROR_16, ///< ROR #16
    ROR_24  ///< ROR #24
};

/**
 * LocationDescriptor describes the location of a basic block.
 * The location is not solely based on the PC because other flags influence the way
 * instructions should be translated. The CPSR.T flag is most notable since it
 * tells us if the processor is in Thumb or Arm mode.
 */
struct LocationDescriptor {
    static constexpr u32 FPSCR_MODE_MASK = 0x03F79F00;

    LocationDescriptor(u32 arm_pc, bool tflag, bool eflag, FPSCR fpscr)
            : arm_pc(arm_pc), tflag(tflag), eflag(eflag), fpscr(fpscr.Value() & FPSCR_MODE_MASK) {}

    u32 PC() const { return arm_pc; }
    bool TFlag() const { return tflag; }
    bool EFlag() const { return eflag; }
    Arm::FPSCR FPSCR() const { return fpscr; }

    bool operator == (const LocationDescriptor& o) const {
        return std::tie(arm_pc, tflag, eflag, fpscr) == std::tie(o.arm_pc, o.tflag, o.eflag, o.fpscr);
    }

    bool operator != (const LocationDescriptor& o) const {
        return !operator==(o);
    }

    LocationDescriptor SetPC(u32 new_arm_pc) const {
        return LocationDescriptor(new_arm_pc, tflag, eflag, fpscr);
    }

    LocationDescriptor AdvancePC(int amount) const {
        return LocationDescriptor(static_cast<u32>(arm_pc + amount), tflag, eflag, fpscr);
    }

    LocationDescriptor SetTFlag(bool new_tflag) const {
        return LocationDescriptor(arm_pc, new_tflag, eflag, fpscr);
    }

    LocationDescriptor SetEFlag(bool new_eflag) const {
        return LocationDescriptor(arm_pc, tflag, new_eflag, fpscr);
    }

    LocationDescriptor SetFPSCR(u32 new_fpscr) const {
        return LocationDescriptor(arm_pc, tflag, eflag, new_fpscr & FPSCR_MODE_MASK);
    }

    u64 UniqueHash() const {
        // This value MUST BE UNIQUE.
        // This calculation has to match up with EmitX64::EmitTerminalPopRSBHint
        u64 pc_u64 = u64(arm_pc);
        u64 fpscr_u64 = u64(fpscr.Value()) << 32;
        u64 t_u64 = tflag ? (1ull << 35) : 0;
        u64 e_u64 = eflag ? (1ull << 39) : 0;
        return pc_u64 | fpscr_u64 | t_u64 | e_u64;
    }

private:
    u32 arm_pc;
    bool tflag;       ///< Thumb / ARM
    bool eflag;       ///< Big / Little Endian
    Arm::FPSCR fpscr; ///< Floating point status control register
};

struct LocationDescriptorHash {
    size_t operator()(const LocationDescriptor& x) const {
        return static_cast<size_t>(x.UniqueHash());
    }
};

const char* CondToString(Cond cond, bool explicit_al = false);
const char* RegToString(Reg reg);
const char* ExtRegToString(ExtReg reg);
std::string RegListToString(RegList reg_list);

inline size_t RegNumber(Reg reg) {
    ASSERT(reg != Reg::INVALID_REG);
    return static_cast<size_t>(reg);
}

inline size_t RegNumber(ExtReg reg) {
    if (reg >= ExtReg::S0 && reg <= ExtReg::S31) {
        return static_cast<size_t>(reg) - static_cast<size_t>(ExtReg::S0);
    } else if (reg >= ExtReg::D0 && reg <= ExtReg::D31) {
        return static_cast<size_t>(reg) - static_cast<size_t>(ExtReg::D0);
    } else {
        ASSERT_MSG(false, "Invalid extended register");
    }
}

inline Reg operator+(Reg reg, size_t number) {
    ASSERT(reg != Reg::INVALID_REG);

    size_t new_reg = static_cast<size_t>(reg) + number;
    ASSERT(new_reg <= 15);

    return static_cast<Reg>(new_reg);
}

inline ExtReg operator+(ExtReg reg, size_t number) {
    ExtReg new_reg = static_cast<ExtReg>(static_cast<size_t>(reg) + number);

    ASSERT((reg >= ExtReg::S0 && reg <= ExtReg::S31 && new_reg >= ExtReg::S0 && new_reg <= ExtReg::S31)
           || (reg >= ExtReg::D0 && reg <= ExtReg::D31 && new_reg >= ExtReg::D0 && new_reg <= ExtReg::D31));

    return new_reg;
}

} // namespace Arm
} // namespace Dynarmic
