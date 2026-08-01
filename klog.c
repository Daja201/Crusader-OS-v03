#include "bootinfo.h"
#include "vesa.h"
#include <stdint.h>
#include "klog.h"
#include "string.h"
#include <stdarg.h>
#include "commands.h"
#include <string.h>
#include "diskinfo.h"
#include "fs.h"
#define MAX_NOTES 15
#define NAME_LEN 20
#define CONTENT_LEN 50
static uint32_t last_free_kb = 0;
static uint32_t log_colors[11];
extern fs_device_t g_drives[];
extern int g_current_drive;
typedef struct {
    char name[NAME_LEN];
    char content[CONTENT_LEN];
    int active;
} Note;
static Note note_list[MAX_NOTES];

static void vprintf_internal(const char *fmt, va_list args) {
    char buf[32];
    char ch;
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            char out[2] = { fmt[i], 0 };
            klog_green(out);
            continue;
        }
        i++;
        switch (fmt[i]) {
            case 'd': {
                int val = va_arg(args, int);
                itoa(val, buf, 10);
                klog_green(buf);
                break;
            }
            case 'x': {
                uint32_t val = va_arg(args, uint32_t);
                itoa(val, buf, 16);
                klog_green(buf);
                break;
            }
            case 's': {
                char *s = va_arg(args, char*);
                if (!s) s = "(null)";
                klog_green(s);
                break;
            }
            case 'c': {
                ch = (char)va_arg(args, int);
                char out[2] = { ch, 0 };
                klog_green(out);
                break;
            }
            case 'l': {
               if (fmt[i+1] == 'l' && fmt[i+2] == 'u') {
                    unsigned long long v = va_arg(args, unsigned long long);
                    int idx = 0;
                    if (v == 0) {
                        buf[idx++] = '0';
                    } else {
                        unsigned long long div = 1;
                        while (div <= v / 10) div *= 10;
                        while (div > 0) {
                            buf[idx++] = '0' + (v / div);
                            v %= div;
                            div /= 10;
                        }
                    }
                    buf[idx] = '\0';
                    klog(buf);
                    i += 2; 
                } else {
                    klog("<?>");
                }
                break;
            }
            case '%':
                klog("%");
                break;
            default:
                klog("<?>");
                break;
        }
    }
}

void klog(const char* msg) {
    while (*msg != '\0') {
        char c = *msg;

        if (c == '\n') {
            cursor('e');
            c_x = 0;
            c_y += 8;
        } 
        else if (c == '\b') {
            cursor('e');    
            if (c_x >= 8) {
                c_x -= 8;
                vesa_draw_char_34(' ', c_x, c_y, 0x000000, 0x000000); 
            }
            else if (c_y >= 8) {
                c_y -= 8;
                c_x = 944;
                vesa_draw_char_34(' ', c_x, c_y, 0x000000, 0x000000);
            }
        }
        else {
            cursor('e');
            vesa_draw_char_34(c, c_x, c_y, 0xFFFFFF, 0x000000);
            c_x += 8;
            if (c_x > 952) {
                c_x = 0;
                c_y += 8;
                vesa_draw_rec(c_x, c_y, 8, 8, 0x000000);
            }
        }

        if (c_y >= 720) {
            c_y = 0;
            vesa_clear(0x000000);
        }

        msg++;
    }
    cursor('d');
}

void cursor(char func) {
    if (func == 'e' || func == 'l') {
        vesa_draw_rec(c_x, c_y, 8, 8, 0x000000);
    }
    else {
        vesa_draw_rec(c_x, c_y + 1, 3, 7, 0xFFFF00);
    }
}

void kklog(const char* msg) {
    klog(msg); 
    klog("\n");
}

void klogf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf_internal(fmt, args);
    va_end(args);
} 

void kklogf(const char *fmt, ...) {
    klogf(fmt);
    klog("\n");  
}

//COLOUR FONTS:
static void vprintf_internal_green_kk(const char *fmt, va_list args) {
    char buf[32];
    char ch;
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            char out[2] = { fmt[i], 0 };
            klog_green(out);
            continue;
        }
        i++;
        switch (fmt[i]) {
            case 'd': {
                int val = va_arg(args, int);
                itoa(val, buf, 10);
                kklog_green(buf);
                break;
            }
            case 'x': {
                uint32_t val = va_arg(args, uint32_t);
                itoa(val, buf, 16);
                klog_green(buf);
                break;
            }
            case 's': {
                char *s = va_arg(args, char*);
                if (!s) s = "(null)";
                kklog_green(s);
                break;
            }
            case 'c': {
                ch = (char)va_arg(args, int);
                char out[2] = { ch, 0 };
                kklog_green(out);
                break;
            }
            case 'l': {
               if (fmt[i+1] == 'l' && fmt[i+2] == 'u') {
                    unsigned long long v = va_arg(args, unsigned long long);
                    int idx = 0;
                    if (v == 0) {
                        buf[idx++] = '0';
                    } else {
                        unsigned long long div = 1;
                        while (div <= v / 10) div *= 10;
                        while (div > 0) {
                            buf[idx++] = '0' + (v / div);
                            v %= div;
                            div /= 10;
                        }
                    }
                    buf[idx] = '\0';
                    klog_green(buf);
                    i += 2; 
                } else {
                    kklog_green("<?>");
                }
                break;
            }
            case '%':
                kklog_green("%");
                break;

            default:
                kklog_green("<?>");
                break;
        }
    }
}

void logo() {
    klog_red("                                                                                              ");
    cmd_time();
    klog_red("     __                                                                                       ");
    drives();
    kklog_red("   ,/ _~.                          |\\                    ,-||-,     -_-/      ------          Welcome to Crusader OS   ");
    kklog_red("  (' /|                       _     \\\\                  ('|||  )   (_ /         ||            An hobby operating system");
    kklog_red(" ((  ||   ,._-_ \\\\ \\\\  _-_,  < \\,  / \\\\  _-_  ,._-_    (( |||--)) (_ --_   |    ||    |       made by:                 ");
    kklog_red(" ((  ||    ||   || || ||_.   /-|| || || || \\\\  ||      (( |||--))   --_ )  |====[]====|       David Zapletal           ");
    kklog_red("  ( / |    ||   || ||  ~ || (( || || || ||/    ||       ( / |  )   _/  ))  |    ||    |                                ");
    kklog_red("   \\____-  \\\\,  \\\\/\\\\ ,-_-   \\/\\\\  \\\\/  \\\\,/   \\\\,       -____-   (_-_-         ||                                     ");
    kklog_red("                                                                              ------                                   ");
    kklog_red("                                DEUS VULT                                                                              ");
    kklog_red("                                                                                                                       ");
    kklog_red("                          Made by github:Daja201                                                                       ");
    kklog_red("                                                                                                                       ");
}

void klogf_green(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf_internal(fmt, args);
    va_end(args);
}

void kklogf_green(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf_internal_green_kk(fmt, args);
    va_end(args);
}

void klog_green(const char* msg) {
    while (*msg != '\0') {
        char c = *msg;
        cursor('e');
        if (c == '\n') {
            c_x = 0;
            c_y += 8;
        } else {

            vesa_draw_char_34(c, c_x, c_y, 0x000000, 0x00FF00);
            c_x += 8;
            if (c_x > 952) {
                c_x = 0;
                c_y += 8;
            }
        }
        if (c_y >= 720) {
            c_y = 0;
            vesa_clear(0x000000);
        }
        msg++; 
    }
    cursor('d');
}

void kklog_green(const char* msg) {
    while (*msg != '\0') {
        char c = *msg;
        cursor('e');
        if (c == '\n') {
            c_x = 0;
            c_y += 8;
        } else {
            vesa_draw_char_34(c, c_x, c_y, 0x000000, 0x00FF00);
            c_x += 8;
            if (c_x > 952) {
                c_x = 0;
                c_y += 8;
            }
        }
        if (c_y >= 720) {
            c_y = 0;
            vesa_clear(0x000000);
        }   
        msg++; 
    }
    cursor('d');
    klog("\n");
}

void klog_red(const char* msg) {
    while (*msg != '\0') {
        char c = *msg;
        if (c == '\n') {
            c_x = 0;
            c_y += 8;
        } else {
            vesa_draw_char_34(c, c_x, c_y, 0xFFFFFF, 0xFF0000);
            c_x += 8;
            if (c_x > 952) {
                c_x = 0;
                c_y += 8;
            }
        }
        if (c_y >= 720) {
            c_y = 0;
            vesa_clear(0x000000);
        }
        msg++;
    }
}

void klog_yellow(const char* msg) {
    while (*msg != '\0') {
        char c = *msg;
        cursor('e');
        if (c == '\n') {
            c_x = 0;
            c_y += 8;
        } else {
            vesa_draw_char_34(c, c_x, c_y, 0xFFFF00, 0x000000);
            c_x += 8;
            if (c_x > 952) {
                c_x = 0;
                c_y += 8;
            }
        }
        if (c_y >= 720) {
            c_y = 0;
            vesa_clear(0x000000);
        }
        msg++;
    }
    cursor('d');
}

void klog_orange(const char* msg) {
    while (*msg != '\0') {
        char c = *msg;
        cursor('e');
        if (c == '\n') {
            c_x = 0;
            c_y += 8;
        } else {
            vesa_draw_char_34(c, c_x, c_y, 0xFFA500, 0x000000);
            c_x += 8;
            if (c_x > 952) {
                c_x = 0;
                c_y += 8;
            }
        }
        if (c_y >= 720) {
            c_y = 0;
            vesa_clear(0x000000);
        }
        msg++;
    }
    cursor('d');
}

void kklog_red(const char* msg) {
    while (*msg != '\0') {
        char c = *msg;
        cursor('e');
        if (c == '\n') {
            c_x = 0;
            c_y += 8;
        } else {
            vesa_draw_char_34(c, c_x, c_y, 0xFFFFFF, 0xFF0000);
            c_x += 8;
            if (c_x > 952) {
                c_x = 0;
                c_y += 8;
            }
        }
        if (c_y >= 720) {
            c_y = 0;
            vesa_clear(0x000000);
        }
        msg++;
    }
    cursor('e');
    klog("\n");
}

static int last_s = -1; 

void clock() {
    int year, month, day, hour, min, sec;
    rtc_get_datetime(&year, &month, &day, &hour, &min, &sec);
    if (sec == last_s) return;
    last_s = sec;
    char b[3];
    int clock_x = 1180;
    int clock_y = 11;
    int time_vals[3] = {hour, min, sec};
    for (int i = 0; i < 3; i++) {
        if (time_vals[i] < 10) {
            vesa_draw_char('0', clock_x, clock_y, 0x2BC7FB, 0x000000);
            clock_x += 8;
        }
        itoa(time_vals[i], b, 10);
        for (int j = 0; b[j] != '\0'; j++) {
            vesa_draw_char(b[j], clock_x, clock_y, 0x2BC7FB, 0x000000);
            clock_x += 8;
        }
        if (i < 2) {
            vesa_draw_char(':', clock_x, clock_y, 0x2BC7FB, 0x000000);
            clock_x += 8;
        }
    }
}

static int last_drive = -1;

void free_ram() {
    static uint32_t last_free_kb = 0; 
    fs_device_t* dev = &g_drives[g_current_drive];
    uint32_t free_kb = pmm_count_mem();
    if (free_kb == last_free_kb && g_current_drive == last_drive) {
        return;
    }
    if (g_current_drive != last_drive) {
        vesa_draw_rec(980, 480, 280, 20, 0x000000); 
    }
    last_free_kb = free_kb;
    last_drive = g_current_drive;
    uint32_t free_mb = free_kb / 1024;
    char buf[32];
    int curr_x;
    int start_x = 980;
    int start_y = 470;
    const char* msg = "Free RAM: ";
    curr_x = start_x;
    for (int i = 0; msg[i] != '\0'; i++) {
        vesa_draw_char(msg[i], curr_x, start_y, 0x2BC7FB, 0x000000);
        curr_x += 8;
    }
    itoa(free_mb, buf, 10);
    for (int i = 0; buf[i] != '\0'; i++) {
        vesa_draw_char(buf[i], curr_x, start_y, 0xFFFFFF, 0x000000);
        curr_x += 8;
    }    
    const char* unit = " MB";
    for (int i = 0; unit[i] != '\0'; i++) {
        vesa_draw_char(unit[i], curr_x, start_y, 0xFFFFFF, 0x000000);
        curr_x += 8;
    }
    uint64_t sectors = dev->total_sectors;
    uint32_t mb = (uint32_t)((sectors * 512) / (1024 * 1024));
    int drive_x = 980;
    int drive_y = 480;
    const char* d_msg = "Current Disk: ";
    curr_x = drive_x;
    for (int i = 0; d_msg[i] != '\0'; i++) {
        vesa_draw_char(d_msg[i], curr_x, drive_y, 0x2BC7FB, 0x000000);
        curr_x += 8;
    }
    itoa(mb, buf, 10);
    for (int i = 0; buf[i] != '\0'; i++) {
        vesa_draw_char(buf[i], curr_x, drive_y, 0xFFFFFF, 0x000000);
        curr_x += 8;
    }
    const char* d_unit = " MB";
    for (int i = 0; d_unit[i] != '\0'; i++) {
        vesa_draw_char(d_unit[i], curr_x, drive_y, 0xFFFFFF, 0x000000);
        curr_x += 8;
    }
}

void gui() {
    const char *title = "CRUSADER OS";
    //bckgrnd
    vesa_draw_rec(0, 0, 1280, 720, 0x000000);
    //JUST A LINE * 3 ↓
    vesa_draw_ver(960, 0, 720, 0xFFFFFF);
    vesa_draw_ver(961, 0, 720, 0xFFFFFF);
    vesa_draw_ver(962, 0, 720, 0xFFFFFF);
    //LOG ↓
    vesa_draw_ver(970, 30, 100, 0xFFFFFF);
    vesa_draw_ver(1270, 30, 100, 0xFFFFFF);
    vesa_draw_hor(970, 30, 300, 0xFFFFFF);
    vesa_draw_hor(970, 130, 300, 0xFFFFFF);
    // NOTES ↓ 
    vesa_draw_ver(970, 140, 310, 0xFFFFFF);
    vesa_draw_ver(1270, 140, 310, 0xFFFFFF);
    vesa_draw_hor(970, 140, 300, 0xFFFFFF);
    vesa_draw_hor(970, 450, 300, 0xFFFFFF);
    // RAM ↓, VERSE ↓, CALENDAR + TODO ↓
    vesa_draw_ver(970, 460, 200, 0xFFFFFF);
    vesa_draw_ver(1270, 460, 200, 0xFFFFFF);
    vesa_draw_hor(970, 460, 300, 0xFFFFFF);
    vesa_draw_hor(970, 660, 300, 0xFFFFFF);
    vesa_draw_hor(970, 550, 300, 0xFFFFFF);
    vesa_draw_hor(970, 500, 300, 0xFFFFFF);
    //APP_red ↓
    vesa_draw_ver(975, 670, 40, 0xFFFFFF);
    vesa_draw_ver(1015, 670, 40, 0xFFFFFF);
    vesa_draw_hor(975, 670, 40, 0xFFFFFF);
    vesa_draw_hor(975, 710, 40, 0xFFFFFF);
    vesa_draw_rec(976, 671, 38, 38, 0xAA0000);
    //APP_blue ↓
    vesa_draw_ver(1015, 670, 40, 0xFFFFFF);
    vesa_draw_ver(1055, 670, 40, 0xFFFFFF);
    vesa_draw_hor(1015, 670, 40, 0xFFFFFF);
    vesa_draw_hor(1015, 710, 40, 0xFFFFFF);
    vesa_draw_rec(1016, 671, 38, 38, 0x0000AA);
    //APP_green ↓
    vesa_draw_ver(1055, 670, 40, 0xFFFFFF);
    vesa_draw_ver(1095, 670, 40, 0xFFFFFF);
    vesa_draw_hor(1055, 670, 40, 0xFFFFFF);
    vesa_draw_hor(1055, 710, 40, 0xFFFFFF);
    vesa_draw_rec(1056, 671, 38, 38, 0x00AA00);
    //APP_yellow ↓
    vesa_draw_ver(1095, 670, 40, 0xFFFFFF);
    vesa_draw_ver(1135, 670, 40, 0xFFFFFF);
    vesa_draw_hor(1095, 670, 40, 0xFFFFFF);
    vesa_draw_hor(1095, 710, 40, 0xFFFFFF);
    vesa_draw_rec(1096, 671, 38, 38, 0xAAAA00);
    //appname ↓
    vesa_draw_ver(1145, 670, 40, 0xFFFFFF);
    vesa_draw_ver(1270, 670, 40, 0xFFFFFF);
    vesa_draw_hor(1145, 670, 125, 0xFFFFFF);
    vesa_draw_hor(1145, 710, 125, 0xFFFFFF);
    //
    int temp_x = 970;
    for (int i = 0; title[i] != '\0'; i++) {
        vesa_draw_char(title[i], temp_x, 11, 0x2BC7FB, 0x000000);
        temp_x += 8;
    }
}

static char log_buffer[11][37];
static int total_logs = 0;

void appname(const char* name) {
    const char *title = name;
    int temp_x = 1150;
    for (int i = 0; title[i] != '\0'; i++) {
        vesa_draw_char(title[i], temp_x, 690, 0x2BC7FB, 0x000000);
        temp_x += 8;
    }
}

void klog_status(const char *status_str, char color_char) {
    uint32_t current_color;
    if (color_char == 'r') {
        current_color = 0xFF0000;

    } 
    else if (color_char == 'g'){
        current_color = 0x00FF00;
    }
    else {
        current_color = 0x2BC7FB;
    }    
    if (total_logs < 11) {
        strncpy(log_buffer[total_logs], status_str, 37 - 1);
        log_buffer[total_logs][36] = '\0';
        log_colors[total_logs] = current_color;
        total_logs++;
    } else {
        for (int i = 0; i < 10; i++) {
            memcpy(log_buffer[i], log_buffer[i + 1], 37);
            log_colors[i] = log_colors[i + 1];
        }
        strncpy(log_buffer[10], status_str, 37 - 1);
        log_buffer[10][36] = '\0';
        log_colors[10] = current_color;
    }
    vesa_draw_rec(972, 32, 296, 88, 0x000000);
    for (int line = 0; line < total_logs; line++) {
        int current_x = 972;
        int current_y = 32 + (line * 8);
        
        for (int i = 0; i < 37 && log_buffer[line][i] != '\0'; i++) {
            vesa_draw_char(log_buffer[line][i], current_x, current_y, log_colors[line], 0x000000);
            current_x += 8;
        }
    }
}

void verse() {
    const char *adress = "John 3:16";
    const char *verse = "For God so loved the world that He gave His only son that everybody who believes in Him would be saved.";
    int temp_x = 980;
    for (int i = 0; adress[i] != '\0'; i++) {
        vesa_draw_char(adress[i], temp_x, 560, 0x4040FF, 0x000000);
        temp_x += 8;
    }
    int temp_v_y = 569;
    int temp_v_x = 980;
    int char_on_line = 0;
    for (int i = 0; verse[i] != '\0'; i++) {
        vesa_draw_char(verse[i], temp_v_x, temp_v_y, 0x2BC7FB, 0x000000);
        temp_v_x += 8;
        char_on_line++;
        if (char_on_line >= 34) {
            temp_v_x = 980;
            temp_v_y += 10;
            char_on_line = 0;
        }
    }
}

void refresh_notes_ui() {
    for (int y = 141; y < 450; y++) {
        for (int x = 971; x < 1270; x++) {
            vesa_putpixel(x, y, 0x000000);
        }
    }
    int start_y = 150;
    for (int i = 0; i < MAX_NOTES; i++) {
        if (note_list[i].active) {
            int curr_x = 975;
            for(int j=0; note_list[i].name[j] != '\0'; j++) {
                vesa_draw_char(note_list[i].name[j], curr_x, start_y, 0x000000, 0xFFFF00);
                curr_x += 8;
            }
            vesa_draw_char(':', curr_x, start_y, 0x000000, 0xFFFFFF);
            curr_x += 16;
            for(int j=0; note_list[i].content[j] != '\0'; j++) {    
                if (curr_x + 8 > 1260) {
                    curr_x = 985; 
                    start_y += 10;
                }
                vesa_draw_char(note_list[i].content[j], curr_x, start_y, 0x000000, 0xFFFFFF);
                curr_x += 8;
            }
            start_y += 14; 
        }
    }
}

void new_note(const char* name, const char* content) {
    for (int i = 0; i < MAX_NOTES; i++) {
        if (!note_list[i].active) {
            strncpy(note_list[i].name, name, NAME_LEN - 1);
            note_list[i].name[NAME_LEN - 1] = '\0'; 
            strncpy(note_list[i].content, content, CONTENT_LEN - 1);
            note_list[i].content[CONTENT_LEN - 1] = '\0';
            note_list[i].active = 1;
            refresh_notes_ui();
            save_notes_to_disk();
            return;
        }
    }
}

void delete_note(const char* name) {
    for (int i = 0; i < MAX_NOTES; i++) {
        if (note_list[i].active && strcmp(note_list[i].name, name) == 0) {
            note_list[i].active = 0;
            refresh_notes_ui();
            return;
        }
    }
}

void delete_all_notes() {
    for (int i = 0; i < MAX_NOTES; i++) {
        note_list[i].active = 0;
    }
    refresh_notes_ui();
}


///

void save_notes_to_disk() {
    inode_t root;
    read_inode(0, &root);
    int inode_idx = dir_lookup(&root, "notes.cos");
    if (inode_idx < 0) {
        inode_idx = fs_create_file("notes.cos", "system");
    }
    if (inode_idx >= 0) {
        fs_write((uint32_t)inode_idx,0, (uint8_t*)note_list, sizeof(note_list));
        klog_status("Notes saved to disk", 'g');
    }
}

void load_notes_from_disk() {
    inode_t root;
    read_inode(0, &root);
    int inode_idx = dir_lookup(&root, "notes.cos");
    if (inode_idx >= 0) {
        inode_t note_inode;
        read_inode(inode_idx, &note_inode);
        fs_read((uint32_t)inode_idx, &note_inode, 0, sizeof(note_list), (uint8_t*)note_list);
        refresh_notes_ui();
        klog_status("Notes loaded", 'g');
    }
}