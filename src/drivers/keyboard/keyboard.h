#ifndef INCLUDE_KEYBOARD_H
#define INCLUDE_KEYBOARD_H

#define KEYBOARD_DATA_PORT 0x60

#define KB_SCAN_CODE_SHIFT 42
#define KB_SCAN_CODE_CTRL 29
#define KB_SCAN_CODE_ALT_GR 56 
#define KB_SCAN_CODE_TOGGLE_SHIFT 58
#define KB_SCAN_CODE_HOME 91

#define KB_SCAN_CODE_SHIFT_RESET 170
#define KB_SCAN_CODE_CTRL_RESET 157
#define KB_SCAN_CODE_ALT_GR_RESET 184 
#define KB_SCAN_CODE_TOGGLE_SHIFT_RESET 186
#define KB_SCAN_CODE_HOME_RESET 219

#define KB_LOWER 0
#define KB_UPPER 1
#define KB_ALT_GR 2

#define KB_BUFFER_SIZE 256
#include "../../filesystem/vfs/vfs.h"
extern generic_file_t kb_file;
/**
 * read_scan_code:
 * Reads a scancode from the keyboard
 * 
 * @return The scan code
 */
uint8_t read_scan_code();

/**
 * decode_scan_code:
 * Decodes a scan code into an ASCII character (includes upper, lower and alt gr characters)
 * 
 * @param scan_code The scan code
 * @return The ASCII character
 */
uint8_t decode_scan_code(uint8_t scan_code);
/**
 * init_keyboard:
 * initalizes keyboard
 */
void init_keyboard();

/**
 * handle_keyboard_interrupt:
 * Handles a keyboard interrupt
 */
void handle_keyboard_interrupt();

/**
 * kb_buffer_pop: 
 * pops the last char from the keyboard buffer
 * @param c The character to pop into
 * @return 1 if successful
 * 
 *         0 if failed
 */
int kb_buffer_pop(unsigned char* c);

/**
 * kb_buffer_is_empty:
 * Returns whether the keyboard char buffer is empty
 * @return 1 if empty
 * 
 *         0 if not
 */
int kb_buffer_is_empty();
#endif