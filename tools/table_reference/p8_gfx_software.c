#include "p8/gfx_software.c"
#include "reference_backend.h"

#define MODULE "p8.gfx_software"

static void font_rows(const char *name, const unsigned char *rows) {
    for (int i = 0; i < P8_FONT_GLYPH_HEIGHT; i++) {
        table_reference_int(MODULE, rows[i], "%s[%d]", name, i);
    }
}

void table_reference_p8_gfx_software(void) {
    for (int i = 0; i < P8_IMAGE_SLOT_COUNT; i++) {
        table_reference_int(MODULE, SLOT_PIXELS[i], "SLOT_PIXELS[%d]", i);
    }
#define ROWS(suffix) font_rows("FONT_ROWS_" #suffix, FONT_ROWS_##suffix);
    ROWS(PLACEHOLDER) ROWS(SPACE)
    ROWS(0) ROWS(1) ROWS(2) ROWS(3) ROWS(4) ROWS(5) ROWS(6) ROWS(7) ROWS(8) ROWS(9)
    ROWS(A) ROWS(B) ROWS(C) ROWS(D) ROWS(E) ROWS(F) ROWS(G) ROWS(H) ROWS(I) ROWS(J) ROWS(K) ROWS(L) ROWS(M)
    ROWS(N) ROWS(O) ROWS(P) ROWS(Q) ROWS(R) ROWS(S) ROWS(T) ROWS(U) ROWS(W) ROWS(X) ROWS(Y) ROWS(Z)
    ROWS(BANG) ROWS(DOLLAR) ROWS(PERCENT) ROWS(APOSTROPHE) ROWS(LPAREN) ROWS(RPAREN) ROWS(PLUS) ROWS(COMMA)
    ROWS(MINUS) ROWS(PERIOD) ROWS(SLASH) ROWS(COLON) ROWS(EQUALS) ROWS(QUESTION)
#undef ROWS
    table_reference_draw_backend(MODULE, "SOFTWARE_BACKEND", &SOFTWARE_BACKEND);
}
