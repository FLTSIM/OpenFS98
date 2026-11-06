/* The raw call boundary preserves signaling-NaN bits on the stack, and
 * observes x87 exception flags before the harness executes any FP code. */
static __attribute__((noinline)) uint64_t clamp_call(uint32_t function, uint64_t bits, uint16_t *status)
{
    uint64_t output;
    uint16_t flags;
    uint32_t low = (uint32_t)bits, high = (uint32_t)(bits >> 32);
    __asm__ volatile("fnclex\n\tpushl %[high]\n\tpushl %[low]\n\t"
                     "call *%[function]\n\tfstpl %[output]\n\tfnstsw %[flags]"
                     : [output] "=m" (output), [flags] "=m" (flags)
                     : [function] "r" (function), [low] "r" (low), [high] "r" (high)
                     : "eax", "ecx", "edx", "cc", "memory", "st");
    *status = flags & 0x3f;
    return output;
}

static int test_clamp(unsigned count)
{
    static const uint64_t edges[] = {
        0, 0x8000000000000000ull, 1, 0x8000000000000001ull,
        0xfffffffffffffull, 0x800fffffffffffffull,
        0x10000000000000ull, 0x8010000000000000ull,
        0x3fffffffffffffffull, 0x4000000000000000ull, 0x4000000000000001ull,
        0x401fffffffffffffull, 0x4020000000000000ull, 0x4020000000000001ull,
        0x7fefffffffffffffull, 0xffefffffffffffffull,
        0x7ff0000000000000ull, 0xfff0000000000000ull,
        0x7ff8000000000000ull, 0xfff8000000000000ull,
        0x7ff0000000000001ull, 0xfff0000000000001ull,
        0x7fffffffffffffffull, 0xffffffffffffffffull
    };
    static const uint16_t precisions[] = {0, 0x200, 0x300};
    unsigned precision, rounding, index, comparisons = 0;
    uint16_t saved, control;
    __asm__ volatile("fnstcw %0" : "=m" (saved));
    for (precision = 0; precision < 3; precision++)
        for (rounding = 0; rounding < 4; rounding++) {
            control = (saved & ~0xf00u) | precisions[precision] | (rounding << 10) | 0x3f;
            __asm__ volatile("fldcw %0" : : "m" (control));
            for (index = 0; index < count + sizeof edges / sizeof edges[0]; index++) {
                uint64_t bits = index < sizeof edges / sizeof edges[0] ? edges[index] :
                                ((uint64_t)random_word() << 32) | random_word();
                uint16_t status[2];
                uint64_t retail = clamp_call(sides[0].code[8], bits, &status[0]);
                uint64_t ours = clamp_call(sides[1].code[8], bits, &status[1]);
                if (retail != ours || status[0] != status[1]) {
                    fprintf(stderr, "clamp mismatch: input %016llx pc %x rc %u: "
                            "%016llx/%x != %016llx/%x\n", (unsigned long long)bits,
                            precisions[precision], rounding, (unsigned long long)retail,
                            status[0], (unsigned long long)ours, status[1]);
                    __asm__ volatile("fnclex\n\tfldcw %0" : : "m" (saved));
                    return 0;
                }
                comparisons++;
            }
        }
    __asm__ volatile("fnclex\n\tfldcw %0" : : "m" (saved));
    printf("CESSSUC clamp: %u vectors across 3 precisions / 4 rounding modes, "
           "result bits and x87 exception flags, 0 differ\n", comparisons);
    return 1;
}
