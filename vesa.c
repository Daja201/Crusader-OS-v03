#include "vesa.h"
#include "font.h"
#include <stdint.h>
#include <string.h>
int c_x = 0;
int c_y = 0;
int vesa_ready = 0;
static volatile uint8_t *lfb = 0;
static uint8_t *back = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_bpp = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_w_34 = 960;

void vesa_init_from_params(uint32_t phys_addr, uint32_t width, uint32_t height, uint32_t bpp, uint32_t pitch) {
    lfb = (volatile uint8_t*)(uintptr_t)phys_addr;
    fb_width = width; 
    fb_height = height;
    fb_bpp = bpp;
    fb_pitch = pitch ? pitch : (width * ((bpp + 7) / 8));
    back = (uint8_t *)0x800000;  
    vesa_ready = 1;
}

static inline void put_pixel_32(int x, int y, uint32_t color) {
    if (x < 0 || x >= 1280 || y < 0 || y >= 720) return;
    uint32_t offset = (y * fb_pitch) + (x * (fb_bpp / 8));
    if (offset >= (1280 * 900 * 4)) return; 
    uint32_t *p = (uint32_t*)(back + offset);
    *p = color;
}

void vesa_draw_char_34(char c, int x, int y, uint32_t fg_color, uint32_t bg_color) {
    if (c < 0 || c > 127) return; 
    const unsigned char *glyph = font8x8_basic[(int)c];
    for (int cy = 0; cy < 8; cy++) {
        unsigned char row = glyph[cy];
        for (int cx = 0; cx < 8; cx++) {
            if (row & (1 << cx)) {
                vesa_putpixel_34(x + cx, y + cy, fg_color);
            } else {
                vesa_putpixel_34(x + cx, y + cy, bg_color);
            }
        }
    }
}

void vesa_putpixel_34(int x, int y, uint32_t color) {
    if (!vesa_ready) return;
    if (x < 0 || (uint32_t)x >= 952 || y < 0 || (uint32_t)y >= fb_height) return;
    if (fb_bpp == 32) put_pixel_32(x,y,color);
    else if (fb_bpp == 24) {
        uint8_t *p = (uint8_t*) ( (uintptr_t)back + (uintptr_t)y * fb_pitch + (uintptr_t)x * 4 );
        p[0] = color & 0xFF;
        p[1] = (color >> 8) & 0xFF;
        p[2] = (color >> 16) & 0xFF;
    } else if (fb_bpp == 8) {
        lfb[y * fb_pitch + x] = (uint8_t)color;
    }
}

void vesa_putpixel(int x, int y, uint32_t color) {
    if (!vesa_ready) return;
    if (x < 0 || (uint32_t)x >= fb_width || y < 0 || (uint32_t)y >= fb_height) return;
    if (fb_bpp == 32) put_pixel_32(x,y,color);
    else if (fb_bpp == 24) {
        uint8_t *p = (uint8_t*) ( (uintptr_t)back + (uintptr_t)y * fb_pitch + (uintptr_t)x * 4 );
        p[0] = color & 0xFF;
        p[1] = (color >> 8) & 0xFF;
        p[2] = (color >> 16) & 0xFF;
    } else if (fb_bpp == 8) {
        lfb[y * fb_pitch + x] = (uint8_t)color;
    }
}

void vesa_clear(uint32_t color) { 
    if (!vesa_ready) return;
    for (uint32_t y = 0; y < fb_height; y++) {
        for (uint32_t x = 0; x < fb_w_34; x++) {
            vesa_putpixel(x, y, color);
        }
    }
    c_x = 0;
    c_y = 0;
}

void vesa_swap(void) {
    if (!vesa_ready || !back) return;
    memcpy((void*)lfb, (void*)back, fb_height * fb_pitch);
}

void vesa_draw_char(char c, int x, int y, uint32_t fg_color, uint32_t bg_color) {
    if (c < 0 || c > 127) return; 
    const unsigned char *glyph = font8x8_basic[(int)c];
    for (int cy = 0; cy < 8; cy++) {
        unsigned char row = glyph[cy];
        for (int cx = 0; cx < 8; cx++) {
            if (row & (1 << cx)) {
                vesa_putpixel(x + cx, y + cy, fg_color);
            } else {
                vesa_putpixel(x + cx, y + cy, bg_color);
            }
        }
    }
}

void vesa_demo(void) {
    if (!vesa_ready) return;
    for (uint32_t y = 0; y < fb_height; y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            uint32_t c = (((x & 0xFF) << 16) | ((y & 0xFF) << 8) | ((x+y)&0xFF));
            vesa_putpixel(x,y,c);
        }
    }
}

void vesa_print_char(char c) {
    if (!vesa_ready) return;
    if (c == '\n') {
        c_x = 0;
        c_y += 8;
        return;
    }
    if (c == '\b') {
        if (c_x >= 8) {
            c_x -= 8;
            vesa_draw_rec(c_x, c_y, 8, 8, 0x000000);
        } else if (c_y >= 8) {
            c_y -= 8;
            c_x = 952;
            vesa_draw_rec(c_x, c_y, 8, 8, 0x000000);
        }
        return;
    }
    if (c == '\r') return;
    
    if (c_x + 8 > 952) {
    c_x = 0;
    c_y += 8;
    }
    vesa_draw_char(c, c_x, c_y, 0xFFFFFF, 0x000000);
    c_x += 8;
}

void vesa_print_string(const char *str) {
    if (!str) return;
    while (*str) {
        vesa_print_char(*str++);
    }
}

void vesa_draw_hor(int x, int y, int a, uint32_t col ) {
    for(int b = 0; b < a; b ++) {
        vesa_putpixel(x + b, y, col);
    }   
}

void vesa_draw_ver(int x, int y, int a, uint32_t col ) {
    for(int b = 0; b < a; b ++) {
        vesa_putpixel(x , y + b, col);
    }   
}

void vesa_draw_rec(int x, int y, int width, int height, uint32_t col ) {
    for(int d = 0; d < height; d ++) {
        for(int c = 0; c < width; c ++) {
            vesa_putpixel(x + c, y + d, col);
        }
    }  
}