// Differential fuzz: TuringMacroJit vs TuringMacroInterpreter (interpreter copy without the
// uninitialized-register check, since the JIT zero-initializes registers).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include "video_core/macro_turing.h"
#include JIT_HEADER

using Word = std::uint32_t;

struct Outcome {
    bool success;
    Word pc;
    std::size_t consumed;
    std::string error;
    std::vector<std::pair<Word, Word>> emits;
    std::vector<Word> reads;
    std::uint64_t dram_hash;
    std::size_t fifo_size;
};

static std::uint64_t Hash(const Word* p, std::size_t n) {
    std::uint64_t h = 1469598103934665603ULL;
    for (std::size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

struct Case {
    std::vector<Word> code;
    std::vector<Word> params;
    Word start;
};

static void SetField(std::array<Word, 3>& bits, Word low, Word count, Word value) {
    for (Word i = 0; i < count; ++i) {
        const Word b = low + i;
        if (value & (1U << i)) bits[b / 32] |= 1U << (b % 32);
    }
}

static Case Generate(std::mt19937& rng) {
    auto r = [&](Word n) { return Word(rng() % n); };
    Case c;
    // Half the cases use only valid fields, so they run deep instead of failing early.
    const bool clean = r(2) == 0;
    static const Word big = std::getenv("BIG") ? Word(std::atoi(std::getenv("BIG"))) : 0;
    const Word count = big ? big - r(16) : 1 + r(r(4) == 0 ? 200 : 24);
    c.code.resize(count * 3);
    auto reg = [&]() -> Word {
        const Word k = r(20);
        if (k < 9) return r(6);           // a few registers, lots of reuse
        if (k < 11) return 24;
        if (k < 14) return 25 + r(3);
        if (k < 18) return 28 + r(2);
        if (k < 19 || clean) return r(24);
        return r(32);
    };
    auto dst = [&]() -> Word {
        const Word k = r(10);
        if (k < 6) return r(6);
        if (k < 9 || clean) return 24;
        return r(32);
    };
    for (Word pc = 0; pc < count; ++pc) {
        std::array<Word, 3> bits{};
        SetField(bits, 0, 1, r(8) == 0 || (clean && pc + 2 >= count && r(2)));
        SetField(bits, 1, 4, r(3) == 0 ? r(16) : 0);
        SetField(bits, 5, 5, reg());
        for (Word lane = 0; lane < 2; ++lane) {
            const Word base = lane * 36;
            Word op = r(32);
            if (r(4) == 0) op = 23 + r(7);
            if (clean && (op == 7 || op == 30 || op == 31) && r(2)) op = 0;
            SetField(bits, 10 + base, 5, op);
            SetField(bits, 15 + base, 5, dst());
            SetField(bits, 20 + base, 5, reg());
            SetField(bits, 25 + base, 5, reg());
            Word imm = rng() & 0xffff;
            if (op >= 23 && op <= 29 && r(5) != 0) {
                // Keep branches and loops mostly inside the program.
                const std::int32_t off = std::int32_t(r(count + 3)) - std::int32_t(pc) - (r(2) ? 0 : 1);
                if (op == 23) imm = r(count + 2);
                else if (op == 24) imm = Word(std::max<std::int32_t>(0, off)) & 0x7fff;
                else {
                    imm = (Word(off) & 0x1fff) | (r(2) ? 0x8000 : 0) | (r(3) == 0 ? 0x4000 : 0);
                    if (r(40) == 0) imm = (imm & 0xe000) | 0x1000; // -4096: stop
                }
            } else if (op == 16 && r(3) != 0) {
                imm = (r(32) << 10) | (r(32) << 5) | r(32);
            } else if (r(3) == 0) {
                imm = r(r(2) ? 0x10 : 0xd00);
            }
            SetField(bits, 30 + base, 16, imm);
        }
        const auto valid_out = [&]() -> Word { const Word v = 1 + r(9); return v == 7 ? 10 : v; };
        const auto outm = [&]() -> Word { return r(3) ? 0 : (clean ? std::min<Word>(valid_out(), 6) : r(8)); };
        const auto oute = [&]() -> Word { const Word k = r(6); return k < 3 ? 0 : (k < 5 || clean ? valid_out() : r(16)); };
        SetField(bits, 82, 3, outm());
        SetField(bits, 85, 4, oute());
        SetField(bits, 89, 3, outm());
        SetField(bits, 92, 4, oute());
        c.code[pc * 3 + 2] = bits[0];
        c.code[pc * 3 + 1] = bits[1];
        c.code[pc * 3 + 0] = bits[2];
    }
    const Word np = clean ? 16 + r(64) : r(24);
    for (Word i = 0; i < np; ++i) c.params.push_back(r(3) ? r(16) : Word(rng()));
    c.start = r(10) == 0 ? r(count + 2) : 0;
    return c;
}

template <typename Run>
static Outcome RunWith(const Case& c, Run run) {
    Outcome o{};
    std::vector<Word> fifo = c.params;
    static Tegra::TuringMacroInterpreter::DataRam dram;
    for (Word i = 0; i < dram.size(); ++i) dram[i] = i * 2654435761U;
    const auto read = [&](Word address) { o.reads.push_back(address); return address * 40503U + 7; };
    const auto emit = [&](Word method, Word value) {
        o.emits.emplace_back(method, value);
        if (method == 0xffe) return false;
        if (method == 0x10 && fifo.size() < 64) fifo.push_back(value ^ 0x5a5a);
        return true;
    };
    const auto result = run(fifo, dram, read, emit);
    o.success = result.success;
    o.pc = result.pc;
    o.consumed = result.consumed;
    o.error = std::string{result.error};
    o.dram_hash = Hash(dram.data(), dram.size());
    o.fifo_size = fifo.size();
    return o;
}

static std::string Describe(const Outcome& o, bool with_pc) {
    char buf[256];
    std::vector<Word> e;
    for (auto& [m, v] : o.emits) { e.push_back(m); e.push_back(v); }
    std::snprintf(buf, sizeof(buf), "s=%d pc=%u c=%zu err=[%s] ne=%zu eh=%llx nr=%zu rh=%llx dram=%llx fifo=%zu",
                  o.success, with_pc ? o.pc : 0, o.consumed, o.error.c_str(), o.emits.size(),
                  (unsigned long long)Hash(e.data(), e.size()), o.reads.size(),
                  (unsigned long long)Hash(o.reads.data(), o.reads.size()),
                  (unsigned long long)o.dram_hash, o.fifo_size);
    return buf;
}

int main(int argc, char** argv) {
    const int cases = argc > 1 ? std::atoi(argv[1]) : 10000;
    const unsigned seed = argc > 2 ? unsigned(std::atoi(argv[2])) : 1;
    std::mt19937 rng{seed};
    int interp_mismatch = 0, success_count = 0, both_success = 0;
    for (int i = 0; i < cases; ++i) {
        const Case c = Generate(rng);
        Tegra::TuringMacroJit jit;
        jit.Prepare(c.code, 1);
        const Outcome j = RunWith(c, [&](std::vector<Word>& fifo, auto& dram, auto& read, auto& emit) {
            return jit.Execute(c.start, fifo, std::span<Word>{dram}, read, emit);
        });
        Tegra::TuringMacroInterpreter interp;
        const Outcome p = RunWith(c, [&](std::vector<Word>& fifo, auto& dram, auto& read, auto& emit) {
            interp.active_dram = &dram;
            return interp.Execute(c.code, c.start, fifo, read, emit);
        });
        std::printf("%d J %s\n", i, Describe(j, true).c_str());
        success_count += j.success;
        if (j.success && p.success) ++both_success;
        const bool same = j.success == p.success &&
                          (!j.success || Describe(j, false) == Describe(p, false));
        if (!same) {
            ++interp_mismatch;
            std::fprintf(stderr, "case %d vs interpreter:\n  J %s\n  I %s\n", i,
                         Describe(j, false).c_str(), Describe(p, false).c_str());
        }
    }
    std::fprintf(stderr, "cases=%d jit_success=%d both_success=%d interp_mismatch=%d\n", cases,
                 success_count, both_success, interp_mismatch);
}
