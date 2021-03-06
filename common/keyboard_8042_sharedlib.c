/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Objects which can be shared between RO and RW for 8042 keyboard protocol.
 */

#include "button.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_config.h"
#include "keyboard_protocol.h"
#include "libsharedobjs.h"
#include "util.h"

/* The standard Chrome OS keyboard matrix table in scan code set 2. */
#ifndef CONFIG_KEYBOARD_SCANCODE_MUTABLE
SHAREDLIB(const
#endif
uint16_t scancode_set2[KEYBOARD_ROWS][KEYBOARD_COLS] = {
	{0x0000, 0xe01f, 0x0005, 0x0032, 0x0009, 0x0051, 0x0031, 0x0000, 0x0055,
	 0x0000, 0xe011, 0x0000, 0x0000},
	{0x0000, 0x0076, 0x000c, 0x0034, 0x0083, 0x0000, 0x0033, 0x0000, 0x0052,
	 0x0001, 0x0000, 0x0066, 0x0064},
	{0x0014, 0x000d, 0x0004, 0x002c, 0x000b, 0x005b, 0x0035, 0x0061, 0x0054,
	 0x000a, 0x006a, 0x0000, 0x0000},
	{0xe01f, 0x000e, 0x0006, 0x002e, 0x0003, 0x0000, 0x0036, 0x0000, 0x004e,
	 0x002f, 0x0000, 0x005d, 0x0067},
	{0xe014, 0x001c, 0x0023, 0x002b, 0x001b, 0x0042, 0x003b, 0x0000, 0x004c,
	 0x004b, 0x005d, 0x005a, 0x0000},
	{0xe007, 0x001a, 0x0021, 0x002a, 0x0022, 0x0041, 0x003a, 0x0012, 0x004a,
	 0x0049, 0x0000, 0x0029, 0x0000},
	{0x0000, 0x0016, 0x0026, 0x0025, 0x001e, 0x003e, 0x003d, 0x0000, 0x0045,
	 0x0046, 0x0011, 0xe072, 0xe074},
	{0x0000, 0x0015, 0x0024, 0x002d, 0x001d, 0x0043, 0x003c, 0x0059, 0x004d,
	 0x0044, 0x0000, 0xe075, 0xe06b},
}
#ifndef CONFIG_KEYBOARD_SCANCODE_MUTABLE
)
#endif
;

/*
 * The translation table from scan code set 2 to set 1.
 * Ref: http://kbd-project.org/docs/scancodes/scancodes-10.html#ss10.3
 * To reduce space, we only keep the translation for 0~127,
 * so a real translation need to do 0x83=>0x41 explicitly (
 * see scancode_translate_set2_to_1 below).
 */
SHAREDLIB(const uint8_t scancode_translate_table[128] = {
	0xff, 0x43, 0x41, 0x3f, 0x3d, 0x3b, 0x3c, 0x58,
	0x64, 0x44, 0x42, 0x40, 0x3e, 0x0f, 0x29, 0x59,
	0x65, 0x38, 0x2a, 0x70, 0x1d, 0x10, 0x02, 0x5a,
	0x66, 0x71, 0x2c, 0x1f, 0x1e, 0x11, 0x03, 0x5b,
	0x67, 0x2e, 0x2d, 0x20, 0x12, 0x05, 0x04, 0x5c,
	0x68, 0x39, 0x2f, 0x21, 0x14, 0x13, 0x06, 0x5d,
	0x69, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0x5e,
	0x6a, 0x72, 0x32, 0x24, 0x16, 0x08, 0x09, 0x5f,
	0x6b, 0x33, 0x25, 0x17, 0x18, 0x0b, 0x0a, 0x60,
	0x6c, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0c, 0x61,
	0x6d, 0x73, 0x28, 0x74, 0x1a, 0x0d, 0x62, 0x6e,
	0x3a, 0x36, 0x1c, 0x1b, 0x75, 0x2b, 0x63, 0x76,
	0x55, 0x56, 0x77, 0x78, 0x79, 0x7a, 0x0e, 0x7b,
	0x7c, 0x4f, 0x7d, 0x4b, 0x47, 0x7e, 0x7f, 0x6f,
	0x52, 0x53, 0x50, 0x4c, 0x4d, 0x48, 0x01, 0x45,
	0x57, 0x4e, 0x51, 0x4a, 0x37, 0x49, 0x46, 0x54,
});


#ifdef CONFIG_KEYBOARD_DEBUG
SHAREDLIB(const
char * const keycap_long_label[KLLI_MAX & KEYCAP_LONG_LABEL_INDEX_BITMASK] = {
	"UNKNOWN", "F1",    "F2",    "F3",
	"F4",      "F5",    "F6",    "F7",
	"F8",      "F9",    "F10",   "F11",
	"F12",     "F13",   "RSVD",  "RSVD",
	"L-ALT",   "R-ALT", "L-CTR", "R-CTR",
	"L-SHT",   "R-SHT", "ENTER", "SPACE",
	"B-SPC",   "TAB",   "SEARC", "LEFT",
	"RIGHT",   "DOWN",  "UP",    "ESC",
});

#ifndef CONFIG_KEYBOARD_SCANCODE_MUTABLE
SHAREDLIB(const
#endif
char keycap_label[KEYBOARD_ROWS][KEYBOARD_COLS] = {
	{KLLI_UNKNO, KLLI_SEARC, KLLI_F1,    'b',        KLLI_F10,
	 KLLI_UNKNO, 'n',        KLLI_UNKNO, '=',        KLLI_UNKNO,
	 KLLI_R_ALT, KLLI_UNKNO, KLLI_UNKNO},
	{KLLI_UNKNO, KLLI_ESC,   KLLI_F4,    'g',        KLLI_F7,
	 KLLI_UNKNO, 'h',        KLLI_UNKNO, '\'',       KLLI_F9,
	 KLLI_UNKNO, KLLI_B_SPC, KLLI_UNKNO},
	{KLLI_L_CTR, KLLI_TAB,   KLLI_F3,    't',        KLLI_F6,
	 ']',        'y',        KLLI_UNKNO, '[',        KLLI_F8,
	 KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO},
	{KLLI_UNKNO, '~',        KLLI_F2,    '5',        KLLI_F5,
	 KLLI_UNKNO, '6',        KLLI_UNKNO, '-',        KLLI_UNKNO,
	 KLLI_UNKNO, '\\',       KLLI_UNKNO},
	{KLLI_R_CTR, 'a',        'd',        'f',        's',
	 'k',        'j',        KLLI_UNKNO, ';',        '|',
	 KLLI_UNKNO, KLLI_ENTER, KLLI_UNKNO},
	{KLLI_UNKNO, 'z',        'c',        'v',        'x',
	 ',',        'm',        KLLI_L_SHT, '/',        '.',
	 KLLI_UNKNO, KLLI_SPACE, KLLI_UNKNO},
	{KLLI_UNKNO, '1',        '3',        '4',        '2',
	 '8',        '7',        KLLI_UNKNO, '0',        '9',
	 KLLI_L_ALT, KLLI_DOWN,  KLLI_RIGHT},
	{KLLI_UNKNO, 'q',        'e',        'r',        'w',
	 'i',        'u',        KLLI_R_SHT, 'p',        'o',
	 KLLI_UNKNO, KLLI_UP,    KLLI_LEFT},
}
#ifndef CONFIG_KEYBOARD_SCANCODE_MUTABLE
)
#endif
;
#endif

uint8_t scancode_translate_set2_to_1(uint8_t code)
{
	if (code & 0x80) {
		if (code == 0x83)
			return 0x41;
		return code;
	}
	return scancode_translate_table[code];
}

/*
 * Button scan codes.
 * Must be in the same order as defined in keyboard_button_type.
 */
SHAREDLIB(const struct button_8042_t buttons_8042[] = {
	{SCANCODE_POWER, 0},
	{SCANCODE_VOLUME_DOWN, 1},
	{SCANCODE_VOLUME_UP, 1},
	{SCANCODE_1, 1},
	{SCANCODE_2, 1},
	{SCANCODE_3, 1},
	{SCANCODE_4, 1},
	{SCANCODE_5, 1},
	{SCANCODE_6, 1},
	{SCANCODE_7, 1},
	{SCANCODE_8, 1},
});
BUILD_ASSERT(ARRAY_SIZE(buttons_8042) == KEYBOARD_BUTTON_COUNT);
