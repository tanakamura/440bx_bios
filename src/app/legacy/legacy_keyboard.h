#ifndef LEGACY_KEYBOARD_H
#define LEGACY_KEYBOARD_H

#include "legacy_rm.h"

void legacy_keyboard_init(void);
void legacy_int16_service(struct rm_int13_frame* f);

#endif
