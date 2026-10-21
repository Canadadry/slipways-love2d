#include "game/cart.c"
#include "table_reference.h"

void table_reference_game_cart(void) {
    const char *module = "game.cart";
    table_reference_int(module, CART_NO_DEMO_FRAME, "CART_NO_DEMO_FRAME");
    table_reference_int(module, (long long)sizeof g_save_bytes, "SIZEOF.g_save_bytes");
    table_reference_int(module, cart_game_registry_reserve.live, "cart_game_registry_reserve.live");
    for (int i = 0; i < ENTITY_MESSAGE_COUNT; i++) {
        table_reference_int(module, cart_game_registry_reserve.by_message[i], "cart_game_registry_reserve.by_message[%d]", i);
    }
    for (int i = 0; i < TAG_COUNT; i++) {
        table_reference_int(module, cart_game_registry_reserve.by_tag[i], "cart_game_registry_reserve.by_tag[%d]", i);
    }
}
