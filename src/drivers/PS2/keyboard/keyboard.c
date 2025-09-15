#include "keyboard.h"
#include "../../../io/io.h"
#include "../../../filesystem/vfs/vfs.h"
#include "../ps2_controller.h"
uint8_t read_scan_code(){
    return inb(PS2_DATA_PORT);
}
static unsigned char kb_buffer[KB_BUFFER_SIZE];
static int kb_head = 0, kb_tail = 0;

void kb_buffer_push(unsigned char c){
  int next = (kb_head + 1) % KB_BUFFER_SIZE; // wrap around
  if (next != kb_tail) {
    kb_buffer[kb_head] = c;
    kb_head = next;
  }
}

int kb_buffer_is_empty(){
  return kb_head == kb_tail;
}

int kb_buffer_pop(unsigned char* c){
  if(kb_head == kb_tail) return 0;
  *c = kb_buffer[kb_tail];
  kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
  return 1;
}

int kb_read(generic_file_t* f, unsigned char* buffer, uint32_t size){
    if (size > KB_BUFFER_SIZE) size = KB_BUFFER_SIZE;

    int read_bytes = 0;
    for (uint32_t i = 0; i < size; i++){
      if (!kb_buffer_pop(&buffer[i])){
        break;
      }
      read_bytes++;
    }

    return read_bytes;
}

vfs_handles_t kb_ops = {
  .close = 0,
  .open = 0,
  .read = kb_read,
  .write = 0,
  .seek = 0,
  .ioctl = 0
};

uint32_t KB_STATUS = KB_LOWER; 
// IMPORTANT: This keyboard map is for a full german keyboard layout, you will need to change it if you have a different layout
// Umlaute have been replaced by their regular counterpart
char keyboard_map_lower[128] = {
  0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 's', '\'', '\b',   
  '\t', /* <-- Tab */
  'q', 'w', 'e', 'r', 't', 'z', 'u', 'i', 'o', 'p', 'u', '+', '\n',     
    0, /*Caps lock */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'o', 'a', /* 40 */ 0, 
  0, /* <-- Shift */ '#',
   'y', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-',  0,
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Alt Gr*/
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};
char keyboard_map_upper[128] = {
  0,27, '!', '"', '$','$', '%','&','/','(',')','=','?','`','\b','\t','Q','W','E','R','T','Z','U','I','O','P','U','*','\n',0,
  'A','S','D','F','G','H','J','K','L','O','A',0,0,'\'','Y','X','C','V','B','N','M',';',':','_'
};
char keyboard_map_ALT_GR[128] = {
  0,27,0,0,0,0,0,0,'{','[',']','}','\\',0,0,0,0,0,0,0,0,0,0,0,0,0,0,'~'
};

void init_keyboard(){
  //values found by logging the scan code and manually reviewing
    keyboard_map_lower[86] = '<';
    keyboard_map_lower[41] = '^';
    keyboard_map_lower[42] = 0; // Shift
    keyboard_map_lower[29] = 0; // STRG
    keyboard_map_lower[56] = 0; // ALT Gr
    keyboard_map_lower[58] = 0; // toggle shift
    keyboard_map_lower[91] = 0; // Home key
    keyboard_map_lower[71] = '7';
    keyboard_map_lower[72] = '8';
    keyboard_map_lower[73] = '9';
    keyboard_map_lower[75] = '4';
    keyboard_map_lower[76] = '5';
    keyboard_map_lower[77] = '6';
    keyboard_map_lower[79] = '1';
    keyboard_map_lower[80] = '2';
    keyboard_map_lower[81] = '3';

    keyboard_map_upper[86] = '>';
    keyboard_map_upper[57] = ' ';

    keyboard_map_ALT_GR[86] = '|';
    keyboard_map_ALT_GR[57] = ' ';

    await_write_signal();
    outb(PS2_CMD_PORT,ENABLE_FIRST_PORT);
}

int is_special_code(uint8_t scan_code){
  return (scan_code == KB_SCAN_CODE_ALT_GR || scan_code == KB_SCAN_CODE_CTRL || scan_code == KB_SCAN_CODE_HOME || scan_code == KB_SCAN_CODE_SHIFT || scan_code == KB_SCAN_CODE_TOGGLE_SHIFT);
}

unsigned char decode_scan_code(uint8_t scan_code){
    switch (scan_code)
    {
    case KB_SCAN_CODE_SHIFT:{

      KB_STATUS = KB_UPPER;
      break;
    }
    case KB_SCAN_CODE_SHIFT_RESET:{

      KB_STATUS = KB_LOWER;
      break;
    }
    case KB_SCAN_CODE_TOGGLE_SHIFT:
      {
        if(KB_STATUS == KB_UPPER){
          KB_STATUS = KB_LOWER;
        }else{
          KB_STATUS = KB_UPPER;
        }
        break;
      } 
    case KB_SCAN_CODE_ALT_GR:{
      KB_STATUS = KB_ALT_GR;
      break;
    }
    case KB_SCAN_CODE_ALT_GR_RESET:{
      KB_STATUS = KB_LOWER;
      break;
    }
    default:
      break;
    }
    
    if (scan_code > 127 || is_special_code(scan_code)){
        return 0;
    }
    if (KB_STATUS == KB_UPPER){
      return keyboard_map_upper[scan_code];
    }
    else if (KB_STATUS == KB_ALT_GR){
      return keyboard_map_ALT_GR[scan_code];
    }
    return keyboard_map_lower[scan_code];
}

void handle_keyboard_interrupt(){
  uint8_t scan_code = read_scan_code();
  unsigned char key = decode_scan_code(scan_code);
  if (key){
    kb_buffer_push(key);
  }
}