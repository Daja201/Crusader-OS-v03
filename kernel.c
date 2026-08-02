#include "klog.h"
#include "bioskbd.h"
#include "terminal.h"
#include "fs.h"
#include "rtc.h"
#include "string.h"
#include "vesa.h"
#include "commands.h"
#include "bootinfo.h"
#include "pmm.h"
#include "bioskbd.h"
#include "idt.h"
#include "task.h"
#include "io.h"
#include "ac97.h"
volatile uint32_t timer_ticks = 0;

void timer_init(uint32_t frequency) {
    uint32_t divisor = 1193182 / frequency;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}
 
void system_main_task() {
    int last_sec = -1;
    for (;;) {
        int needs_redraw = 0;
        if (bios_has_char()) { 
            char c = bios_getchar_echo(); 
            if (c != 0) {
                terminal_key(c);
                needs_redraw = 1;
            }
        }
        int y, m, d, h, min, sec;
        rtc_get_datetime(&y, &m, &d, &h, &min, &sec);
        if (sec != last_sec) {
            last_sec = sec;
        }
        if (needs_redraw) {
            cursor('d');
            vesa_swap();
        }
        asm volatile("pause");
    }
}

void kmain(unsigned long mb_magic, unsigned long mb_info) {
    parse_multiboot((uint32_t)mb_magic, (uint32_t)mb_info);
    pmm_init(); 
    pmm_init_region(0x2000000, 0xFF000000);
    init_idt();
    vesa_init_from_params(boot_fb_addr, boot_fb_width, boot_fb_height, boot_fb_bpp, boot_fb_pitch);
    extern void init_paging(uint32_t, uint32_t, uint32_t, uint32_t);
    init_paging(boot_fb_addr, boot_fb_width, boot_fb_height, boot_fb_bpp);
    vesa_clear(0x000000);
    c_x = 0;
    c_y = 0;
    logo();
    init_fs();
    klog("\n");
    klog_color("CRUSADER>> ", 0xFFFF00);
    klog_status("BOOTED", 0x0000FF);
    timer_init(1000); 
    init_multitasking();
    create_task(system_main_task); 
    klog_status("MULTITASKING STARTED", 0x0000FF);
    ac97_init();
    asm volatile("sti");
    while (1) {
        asm volatile("hlt");
    }
}