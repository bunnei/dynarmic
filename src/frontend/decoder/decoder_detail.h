/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * This software may be used and distributed according to the terms of the GNU
 * General Public License version 2 or any later version.
 */

#pragma once

#include <algorithm>
#include <array>
#include <tuple>

#include "common/assert.h"
#include "common/mp.h"

namespace Dynarmic {
namespace Arm {
namespace detail {

/**
 * Helper functions for the decoders.
 * @tparam MatcherT The type of the matcher. (ARM: ArmMatcher, Thumb16: Thumb16Matcher)
 * @tparam InstructionT The type that represents an instruction. (ARM: u32, Thumb16: u16)
 * @tparam instruction_bit_size Bit-size for an instruction. (ARM: 32, Thumb16: 16)
 */
template<template<typename> class MatcherT, typename InstructionT, size_t instruction_bit_size>
struct detail {
private:
    /**
     * Generates the mask and the expected value after masking from a given bitstring.
     * A '0' in a bitstring indicates that a zero must be present at that bit position.
     * A '1' in a bitstring indicates that a one must be present at that bit position.
     */
    static auto GetMaskAndExpect(const char* const bitstring) {
        InstructionT mask = 0, expect = 0;
        for (size_t i = 0; i < instruction_bit_size; i++) {
            const size_t bit_position = instruction_bit_size - i - 1;
            switch (bitstring[i]) {
            case '0':
                mask |= 1 << bit_position;
                break;
            case '1':
                expect |= 1 << bit_position;
                mask |= 1 << bit_position;
                break;
            default:
                // Ignore
                break;
            }
        }
        return std::make_tuple(mask, expect);
    }

    /**
     * Generates the masks and shifts for each argument.
     * A '-' in a bitstring indicates that we don't care about that value.
     * An argument is specified by a continuous string of the same character.
     */
    template<size_t N>
    static auto GetArgInfo(const char* const bitstring) {
        std::array<InstructionT, N> masks = {};
        std::array<size_t, N> shifts = {};
        size_t arg_index = 0;
        char ch = 0;

        for (size_t i = 0; i < instruction_bit_size; i++) {
            const size_t bit_position = instruction_bit_size - i - 1;

            if (bitstring[i] == '0' || bitstring[i] == '1' || bitstring[i] == '-') {
                if (ch != 0) {
                    ch = 0;
                    arg_index++;
                }
            } else {
                if (ch == 0) {
                    ch = bitstring[i];
                } else if (ch != bitstring[i]) {
                    ch = bitstring[i];
                    arg_index++;
                }

                ASSERT(arg_index < N);
                masks[arg_index] |= 1 << bit_position;
                shifts[arg_index] = bit_position;
            }
        }

        ASSERT(std::all_of(masks.begin(), masks.end(), [](auto m){ return m != 0; }));

        return std::make_tuple(masks, shifts);
    }

    /**
     * This struct's Make member function generates a lambda which decodes an instruction based on
     * the provided arg_masks and arg_shifts. The Visitor member function to call is provided as a
     * template argument.
     */
    template<typename FnT>
    struct VisitorCaller;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4800) // forcing value to bool 'true' or 'false' (performance warning)
#endif
    template<typename Visitor, typename ...Args, typename CallRetT>
    struct VisitorCaller<CallRetT(Visitor::*)(Args...)> {
        template<size_t ...iota>
        static auto Make(std::integer_sequence<size_t, iota...>,
                         CallRetT (Visitor::* const fn)(Args...),
                         const std::array<InstructionT, sizeof...(iota)> arg_masks,
                         const std::array<size_t, sizeof...(iota)> arg_shifts) {
            return [fn, arg_masks, arg_shifts](Visitor& v, InstructionT instruction) {
                (void)instruction;
                return (v.*fn)(static_cast<Args>((instruction & arg_masks[iota]) >> arg_shifts[iota])...);
            };
        }
    };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

public:
    /**
     * Creates a matcher that can match and parse instructions based on bitstring.
     * See also: GetMaskAndExpect and GetArgInfo for format of bitstring.
     */
    template<typename FnT>
    static auto GetMatcher(FnT fn, const char* const name, const char* const bitstring) {
        using Visitor = typename mp::FunctionInfo<FnT>::class_type;
        constexpr size_t args_count = mp::FunctionInfo<FnT>::args_count;
        using Iota = std::make_index_sequence<args_count>;

        const auto mask_and_expect = GetMaskAndExpect(bitstring);
        const auto arg_info = GetArgInfo<args_count>(bitstring);
        const auto proxy_fn = VisitorCaller<FnT>::Make(Iota(), fn, std::get<0>(arg_info), std::get<1>(arg_info));

        return MatcherT<Visitor>(name, std::get<0>(mask_and_expect), std::get<1>(mask_and_expect), proxy_fn);
    }
};

} // namespace detail
} // namespace Arm
} // namespace Dynarmic
