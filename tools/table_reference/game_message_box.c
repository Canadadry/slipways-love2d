#include "game/message_box.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_message_box(void) {
    const char *module = "game.message_box";
    reference_class(module, "message_box_class_def", &message_box_class_def);
    reference_class(module, "toast_class_def", &toast_class_def);
    table_reference_int(module, MESSAGE_BOX_ITEMS_CAP, "MESSAGE_BOX_ITEMS_CAP");
    table_reference_int(module, MESSAGE_BOX_ARGUMENTS_CAP, "MESSAGE_BOX_ARGUMENTS_CAP");
    table_reference_int(module, MESSAGE_BOX_TEXT_CAP, "MESSAGE_BOX_TEXT_CAP");
    table_reference_int(module, MESSAGE_BOX_ARGUMENT_NUMBER, "MESSAGE_BOX_ARGUMENT_NUMBER");
    table_reference_int(module, MESSAGE_BOX_ARGUMENT_FALSE, "MESSAGE_BOX_ARGUMENT_FALSE");
    table_reference_int(module, MESSAGE_BOX_ARGUMENT_NIL, "MESSAGE_BOX_ARGUMENT_NIL");
    table_reference_int(module, MESSAGE_BOX_ARGUMENT_TEXT, "MESSAGE_BOX_ARGUMENT_TEXT");
    table_reference_int(module, MESSAGE_BOX_ARTICLE_SCALARS, "MESSAGE_BOX_ARTICLE_SCALARS");
    table_reference_int(module, MESSAGE_BOX_ARTICLE_LAYOUT_ROW, "MESSAGE_BOX_ARTICLE_LAYOUT_ROW");
    table_reference_int(module, MESSAGE_BOX_ARTICLE_SHARED_ROW, "MESSAGE_BOX_ARTICLE_SHARED_ROW");
    table_reference_int(module, TOAST_TEXT_CAP, "TOAST_TEXT_CAP");
    table_reference_int(module, OPEN_BOX_DESCRIPTION_CAP, "OPEN_BOX_DESCRIPTION_CAP");
    table_reference_int(module, (long long)sizeof(message_box_t), "SIZEOF.message_box_t");
    table_reference_int(module, (long long)sizeof(toast_t), "SIZEOF.toast_t");
}
