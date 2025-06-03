#define VIDEO_MEMORY_START 0xb8000

#define SCREEN_ROWS 25
#define SCREEN_COLUMNS 80

#define COLOR_BLACK 0x0
#define COLOR_BLUE 0x1
#define COLOR_GREEN 0x2
#define COLOR_CYAN 0x3
#define COLOR_RED 0x4
#define COLOR_MAGENTA 0x5
#define COLOR_BROWN 0x6
#define COLOR_LIGHT_GREY 0x7
#define COLOR_DARK_GREY 0x8
#define COLOR_LIGHT_BLUE 0x9
#define COLOR_LIGHT_GREEN 0xa
#define COLOR_LIGHT_CYAN 0xb
#define COLOR_LIGHT_RED 0xc
#define COLOR_LIGHT_MAGENTA 0xd
#define COLOR_LIGHT_BROWN 0xe
#define COLOR_WHITE 0xf

char* fb = (char*)VIDEO_MEMORY_START;
/**
*  fb_write_cell:
*  writes a char with given foreground and background color to position i in the framebuffer
* 
* @param i The location in the framebuffer
* @param c The char
* @param fg The foreground color
* @param bg The background color
*  
* */ 
void fb_write_cell(unsigned int i, char c,unsigned char fg, unsigned char bg){
    fb[i * 2] = c;
    fb[i * 2 +1] = ((bg & 0x0f) << 4 | (fg & 0x0f));
}
/**
*
* Clears the screen of the OS terminal
*
*/
void clear_screen(){
    unsigned int i = 0;
    while (i <  SCREEN_ROWS * SCREEN_COLUMNS){
        fb_write_cell(i,' ',COLOR_BLACK,COLOR_BLACK);
        i++;
    }
}
/**
 * Writes a string to the OS terminal
 * 
 * @param str The string
 * @param len The length of the string
 */
void write_string_n(const char* str, const unsigned int len){
    unsigned int i = 0;
    while(i < len){
        fb_write_cell(i,str[i],COLOR_LIGHT_GREY,COLOR_BLACK);
        i++;
    }
}
void kmain(void)
{
    const char* msg = "Hello BrickOS!";
    clear_screen();
    write_string_n(msg,15);
    return;
}