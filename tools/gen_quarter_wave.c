/* Host tool behind make sin-table: writes src/p8/quarter_wave.h on stdout.
 * Each entry is the floating-point sine p8_fixed_sin/cos computed before the
 * table, standard sign, at its index / 65536 of a turn: same 2*pi, same
 * rounding as fixed_test_double_from_double. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define TWO_PI 6.28318530717958647692528676655900576839433879875021
#define QUARTER_WAVE_ENTRIES 16385

static int32_t round_to_fixed(double d) {
    double scaled = d * 65536.0;
    int64_t raw = (int64_t)(scaled >= 0 ? scaled + 0.5 : scaled - 0.5);
    return (int32_t)raw;
}

int main(void) {
    int r;

    printf("#ifndef P8_QUARTER_WAVE_H\n");
    printf("#define P8_QUARTER_WAVE_H\n\n");
    printf("#include <stdint.h>\n\n");
    printf("static const int32_t p8_quarter_wave[%d] = {\n", QUARTER_WAVE_ENTRIES);
    for (r = 0; r < QUARTER_WAVE_ENTRIES; r++) {
        double turns = (double)r / 65536.0;
        double radians = turns * TWO_PI;
        int32_t v = round_to_fixed(sin(radians));
        printf("%s0x%08lX,", (r % 8 == 0) ? "    " : " ", (unsigned long)(uint32_t)v);
        if (r % 8 == 7 || r == QUARTER_WAVE_ENTRIES - 1) {
            printf("\n");
        }
    }
    printf("};\n\n#endif\n");
    return 0;
}
