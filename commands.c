#include "commands.h"
#include "terminal.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <cpuid.h>
#include "reboot.h"
#include "diskinfo.h"
#include "fs.h"
#include "library.h"
#include "klog.h"
#include "string.h"
#include "fs.h"
#include "rtc.h"
#include "vesa.h"
#include "pmm.h"
#include "pci.h"
#include "app.h"
#include "ac97.h"
#include "speaker.h"
#include "task.h"
#include "templar.h"
#define CHUNK_SIZE 65532
//extern pci_device_t g_dev;
extern fs_device_t g_drives[MAX_DRIVES];
extern int g_current_drive;
extern int g_active_drives;
extern superblock_t g_superblock;
extern void select_drive(uint16_t base, uint8_t slave);
extern void block_read(uint32_t lba, uint8_t* buf);
extern int fs_change_drive(int drive_id);
extern void init_fs(void);
extern char g_current_path[];
extern uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
extern void pci_config_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
//extern pci_device_t g_dev;
extern uint32_t g_current_dir;
extern void read_inode(int idx, inode_t* inode);
extern int fs_resolve_path(const char* path, uint32_t current_dir_inode);
static uint8_t wav_audio_buffer[65536] __attribute__((aligned(8))); 
extern int fs_resolve_path(const char* path, uint32_t current_dir_inode);
static uint8_t streaming_buffer[CHUNK_SIZE] __attribute__((aligned(8)));

struct ac97_bdl_entry {
    uint32_t buffer_addr;
    uint16_t length;
    uint16_t flags;
} __attribute__((packed));

static struct ac97_bdl_entry bdl[1] __attribute__((aligned(8)));
static int16_t audio_buffer[32000]; 


extern void select_drive(uint16_t base, uint8_t slave);
extern void block_read(uint32_t lba, uint8_t* buf);

#define CHUNK_SECTORS 128 // Budeme číst 64 KB (128 sektorů * 512 B)
static uint8_t raw_wav_buffer[CHUNK_SECTORS * 512] __attribute__((aligned(8)));


void cmd_help(int argc, char** argv) {
    kklog_red("Welcome to Crusader OS made by David Zapletal");
    kklog_red("Source code shoould be available on: https://github.com/Daja201/Crusader-OS-v02");
    kklog_red("Feel free to copy and change source code for yourself.");
    kklog_red("Run command: 'lib' for info about commands and whole system.");
}

void busy_ms(int ms) {
    volatile unsigned int iter = 8000 * ms;
    for (volatile unsigned int i = 0; i < iter; i++) asm volatile ("nop");
}

//YAII FINALLY JUPMING COW (Thanks to Lujza <3 for whole idea)
void cmd_cow(int argc, char** argv) {
    vesa_clear(0x000000);
    vesa_draw_rec(0, 714, 960, 6, 0x00FF00);
    vesa_swap();
    const char *cow[] = {
        "          (__) ",
        "          (oo) ",
        "   /-------\\/  ",
        "  / |     ||   ",
        " +  ||----||   ",
        "    ^^    ^^   "
    };
    const int cow_lines = 6;
    const int char_w = 8;
    const int char_h = 8;
    int cow_width_px = 15 * char_w;
    int x_start = (952 - cow_width_px) / 2;
    if (x_start < 0) x_start = 0;
    int top_y = 10;
    int bottom_y = 665;
    const int frames_per_jump = 30;    
    const int ms_per_frame = 60; 
    int prev_y = top_y;

    for (int iteration = 0; iteration < 10; iteration++) {
        for (int step = 0; step <= frames_per_jump; step++) {
            int current_y = top_y + ((bottom_y - top_y) * step / frames_per_jump);
            vesa_draw_rec(x_start, prev_y, cow_width_px, cow_lines * char_h, 0x000000);
            for (int i = 0; i < cow_lines; i++) {
                int line_y = current_y + (i * char_h);
                for (int j = 0; cow[i][j] != '\0'; j++) {
                    vesa_draw_char(cow[i][j], x_start + (j * char_w), line_y, 0xFFFFFF, 0x000000);
                }
            }
            vesa_swap();
            busy_ms(ms_per_frame);
            prev_y = current_y;
        }
        for (int step = frames_per_jump; step >= 0; step--) {
            int current_y = top_y + ((bottom_y - top_y) * step / frames_per_jump);    
            vesa_draw_rec(x_start, prev_y, cow_width_px, cow_lines * char_h, 0x000000);    
            for (int i = 0; i < cow_lines; i++) {
                int line_y = current_y + (i * char_h);
                for (int j = 0; cow[i][j] != '\0'; j++) {
                    vesa_draw_char(cow[i][j], x_start + (j * char_w), line_y, 0xFFFFFF, 0x000000);
                }
            }
            vesa_swap();
            busy_ms(ms_per_frame);
            prev_y = current_y;
        }
    }
    vesa_clear(0x000000);
    vesa_swap();
}


void cmd_mem() {
    uint32_t free_kb = pmm_count_mem();
    klogf("Free memory: %d KB (%d MB)\n", free_kb, free_kb / 1024);
}

void cmd_cat(int argc, char** argv) {
    const char *cat =
"    /\\___/\\   \n"
"   /       \\  \n"
"  |  u   u  | \n"
"--|----*----|--\n"
"   \\   w   /       \n"
"     ======\n"
"   /       \\ __   \n"
"   |        |\\ \\   \n"
"   |        |/ /     \n"
"   |  | |   | /   \n"
"   \\ ml lm /_/      \n";
    vesa_print_string(cat);
}

void cmd_ld(int argc, char** argv) {
    if (g_active_drives == 0) {
        kklog_red("No active drives detected.");
        return;
    }
    kklog_green("ALL DRIVES INFO:");
    int original_drive = g_current_drive;
    for (int i = 0; i < g_active_drives; i++) {
        fs_device_t* dev = &g_drives[i];
        klogf("Drive Index:  %d\n", i);
        klogf("ATA Base:     0x%x\n", (uint32_t)dev->ata_base);
        klogf("Type:         %s\n", dev->is_slave ? "Slave" : "Master");
        uint64_t sectors = dev->total_sectors;
        uint64_t mb = (sectors * 512) / (1024 * 1024);
        klogf("Capacity:     %d MB (%d sectors)\n", (uint32_t)mb, (uint32_t)sectors);
        select_drive(dev->ata_base, dev->is_slave);
        uint8_t sector_buf[512];
        block_read(0, sector_buf);
        superblock_t* temp_sb = (superblock_t*)sector_buf;
        if (temp_sb->magic == 0x5A4C534A) {
            if (i == original_drive) {
                kklog_green("Status:       Formatted [CURRENT]");
            } else {
                kklog_green("Status:       Formatted");
            }
            klogf("Total Blocks: %d\n", temp_sb->total_blocks);
            klogf("Inodes:       %d used/total\n", temp_sb->inode_count);
            klogf("Data Start:   LBA %d\n", temp_sb->data_start);
        } else {
            if (i == original_drive) {
                kklog_red("Status:       NOT FORMATTED [CURRENT]");
                kklog("use 'format' to format current drive");
            } else {
                kklog_red("Status:       NOT FORMATTED");
            }
            
        }    
        klog("\n");
        vesa_draw_hor(c_x, c_y + 4, 500, 0xFFFFFF);
        c_y = c_y + 8;
    }
    select_drive(g_drives[original_drive].ata_base, g_drives[original_drive].is_slave);
}

void cmd_clear(int argc, char** argv) {
    vesa_clear(0x000000);
}

void cmd_reboot(int argc, char** argv) {
    kklog("reboot in process\n");
    reboot_triple_fault();
    
}

void cmd_lib(int argc, char** argv) {
    library();
}

void cmd_read(int argc, char** argv) {
    if (argc < 2) {
        kklog("Usage: read <filename>");
        return;
    }
    inode_t root;
    read_inode(g_current_dir, &root);
    int inode_num = dir_lookup(&root, argv[1]);
    if (inode_num < 0) {
        kklog("Error: File not found");
        return;
    }
    inode_t file_node;
    read_inode(inode_num, &file_node);
    uint8_t buf[32768];
    uint32_t to_read = file_node.size;
    if (to_read > sizeof(buf)) {
        to_read = sizeof(buf); 
    }
    int bytes_read = fs_read(inode_num, &file_node, 0, to_read, buf);
    if (bytes_read <= 0) {
        kklog("Error: Could not read file (or empty)");
        return;
    }
    for (int i = 0; i < bytes_read; i++) {
        if (buf[i] == '\0') break;
        char ch[2] = {buf[i], '\0'};
        vesa_print_string(ch);
    }
    vesa_print_string("\n");
}

void cmd_ls(int argc, char** argv) {
    inode_t dir;
    read_inode(g_current_dir, &dir); 
    uint8_t buf[512];
    struct dirent {
        uint32_t inode;
        char name[28];
    };
    int entry_count = 512 / sizeof(struct dirent);
    for (int b = 0; b < 12; b++) {
        uint32_t block_lba = dir.direct[b];
        if (block_lba == 0) continue;
        block_read(block_lba, buf);
        struct dirent* entries = (struct dirent*)buf;
        for (int i = 0; i < entry_count; i++) {
            if (entries[i].inode != 0) {
                inode_t file_node;
                read_inode(entries[i].inode, &file_node);
                klog("  ");
                if (file_node.type == 2) {
                    klog_red(entries[i].name);
                } else {
                    klog_green(entries[i].name);
                }
            }
        }
        klog("\n");
    }
}

void cmd_dl(int argc, char** argv) {
    if (argc < 2) {
        kklog("usage: dl <file>");
        return;
    }
    if (fs_delete_file(argv[1]) < 0) {
        kklog("dl: failed");
        return;
    }
    kklog("file deleted");
}

void cmd_wr(int argc, char** argv) {
    if (argc < 3) {
        kklog("Usage: wr <filename> <data>");
        return;
    }
    const char* wr = "wr";
    const char* filename = argv[1]; 
    const char* data = argv[2];
    uint32_t inode = fs_create_file(filename, wr);
    if ((int32_t)inode < 0) {
        kklog("file creation failed");
        return;
    }
    inode_t node;
    read_inode(inode, &node);
    int written = fs_write(inode, node.size,(const uint8_t*)data, strlen(data));
    if (written < 0) {
        kklog("write failed");
    } else {
        kklog("write successful");
    }
}

void cmd_shutdown(int argc, char** argv) {
    kklog_red("\nPREPARING FS...\n");
    save_block_bitmap();
    save_inode_bitmap();
    busy_ms(1000);
    asm volatile ("cli");
    vesa_draw_rec(0, 0, 1280, 1024, 0x000000);    
    const char* verse = "When you lie down, you will not be afraid. Yes, you will lie down, and your sleep will be sweet. (PROVERBS 3:24)";
    const char* msg = "IT IS NOW SAFE TO TURN OFF YOUR COMPUTER. GOOD NIGHT :)";
    int start_x_msg = (1280 - (strlen(msg) * 8)) / 2;
    int start_y_msg = 350;
    int start_x_verse = (1280 - (strlen(verse) * 8)) / 2;
    int start_y_verse = start_y_msg - 20;
    for(int i = 0; verse[i] != '\0'; i++) {
        vesa_draw_char(verse[i], start_x_verse + (i * 8), start_y_verse, 0x2BC7FB, 0x000000); 
    }
    for(int i = 0; msg[i] != '\0'; i++) {
        vesa_draw_char(msg[i], start_x_msg + (i * 8), start_y_msg, 0x2BC7FB, 0x000000); 
    }
    vesa_swap();
    for (;;) {
        asm volatile ("hlt");
    }
}

void cmd_time(int argc, char** argv) {
    int year, month, day;
    int hour, min, sec;
    rtc_get_datetime(&year, &month, &day, &hour, &min, &sec);
    char b[8];
    klog_green("RTC: ");
    itoa(year, b, 10); klog_green(b);;itoa(month, b, 10);klog_green(" "); klog_green(b);itoa(day, b, 10);klog_green(" "); klog_green(b); klog_green(" ");
    itoa(hour, b, 10); klog_green(b); klog_green(":");
    itoa(min, b, 10); klog_green(b); klog_green(":"); 
    itoa(sec, b, 10); klog_green(b);
    klog_green("\n");
}

void cmd_note(int argc, char** argv) {
    if (argc < 3) {
        kklog("usage: note <c/d> <name> <content>\n");
        return;
    }
    char action = argv[1][0];
    const char* name = argv[2];
    if (action == 'c') {
        if (argc < 4) {
            kklog("usage: note c <name> <content>\n");
            return;
        }
        const char* content = argv[3];
        new_note(name, content);
    } 
    else if (action == 'd') {
        if (strcmp(name, "all") == 0) {
            delete_all_notes();
        } 
        else {
            delete_note(name);
        }
    } 
    else {
        kklog("usage: note <c/d> <name> <content>\n"); 
    }
}

void cmd_format(int argc, char** argv) {
    format_fs();
}

void cmd_qformat(int argc, char** argv) {
    qformat_fs();
}

void cmd_usedisk(int argc, char** argv) {
    if (argc < 2 || argv[1] == NULL || argv[1][0] == '\0' || argv[1][0] == ' ') {
        kklog("Usage: use <drive_index>\n       use <port> <0|1> (port: 0=primary,1=secondary or 0x1F0/0x170)\n");
        return;
    }
    if (argc == 2) {
        int drive_index = atoi(argv[1]);
        if (drive_index < 0 || drive_index >= g_active_drives) {
            kklog("Error: Invalid drive index.\n");
            return;
        }
        if (fs_change_drive(drive_index) == 0) {
            select_drive(g_drives[drive_index].ata_base, g_drives[drive_index].is_slave);
            klogf("Current drive now: %d", g_current_drive);
        } else {
            kklog("Error: Failed to change drive\n");
        }
        return;
    }
    const char* port_arg = argv[1];
    const char* unit_arg = argv[2];
    uint16_t base = 0;
    if (strcmp(port_arg, "0") == 0) base = 0x1F0;
    else if (strcmp(port_arg, "1") == 0) base = 0x170;
    else base = (uint16_t)strtol(port_arg, NULL, 0);
    int slave = 0;
    if (unit_arg[0] == '1' || strcasecmp(unit_arg, "slave") == 0 || strcasecmp(unit_arg, "s") == 0) slave = 1;
    int found = -1;
    for (int i = 0; i < g_active_drives; i++) {
        if (g_drives[i].ata_base == base && g_drives[i].is_slave == (uint8_t)slave) { found = i; break; }
    }
    if (found >= 0) {
        if (fs_change_drive(found) == 0) {
            select_drive(g_drives[found].ata_base, g_drives[found].is_slave);
            klogf("Switched to drive index %d\n", found);
            klogf("Current drive now: %d", g_current_drive);
        } else {
            kklog("Error: Failed to change drive\n");
        }
        return;
    }
    select_drive(base, (uint8_t)slave);
    if (g_active_drives < MAX_DRIVES) {
        int new_idx = g_active_drives;
        g_drives[new_idx].ata_base = base;
        g_drives[new_idx].is_slave = (uint8_t)slave;
        g_drives[new_idx].total_sectors = 0;
        g_current_drive = new_idx;
        g_active_drives++;
        init_fs();
        select_drive(g_drives[new_idx].ata_base, g_drives[new_idx].is_slave);
        g_current_drive = new_idx;
        klogf("Selected ATA base 0x%x slave %d (added as index %d)\n", (uint32_t)base, slave, g_current_drive);
    } else {
        klogf("Selected ATA base 0x%x slave %d\n", (uint32_t)base, slave);
    }
}

void cmd_find(int argc, char** argv) {
    if (argc < 2) {
        kklogf("usage: find <tag>\n");
        return;
    }
    const char* tag = argv[1];
    uint32_t results[32];
    int found_count = fs_find_by_tag(tag, results, 32);
    if (found_count <= 0) {
            kklogf("No files found with tag: %s", tag);
        } else {
            kklogf("Found %d files:", found_count);
        for (int i = 0; i < found_count; i++) {
            kklogf(" - Inode: %d", results[i]);
        }
    }
    klog("\n");
}

void cmd_app(int argc, char** argv) {
    if (argc < 2) {
        kklog("Usage: app <app_name>");
        return;
    }
    app(argv[1]);
}

void cmd_play97(int argc, char** argv) {
    if (ac97_init() == 0) {
        ac97_play_test_tone();
    }
}

void cmd_mf(int argc, char** argv) {
    if (argc < 2) {
        kklog("Usage: mf <name>");
        return;
    }
    if (fs_create_dir(argv[1], g_current_dir) == -1) {
        kklog("Failed to create directory.");
    }
}

void cmd_cd(int argc, char** argv) {
    if (argc < 2) {
        kklogf("Current directory inode: %d", g_current_dir);
        return;
    }

    int result = fs_cd(argv[1]);
    if (result == 0) {
        if (strcmp(argv[1], "..") == 0) {
            strcpy(g_current_path, ">");
        } else {
            strcpy(g_current_path, ">");
            strcat(g_current_path, argv[1]); 
        }
    } else if (result == -2) {
        kklog("Error: Not a directory.");
    } else {
        kklog("Error: Directory not found.");
    }
}

void cmd_open(int argc, char** argv) {
    if (argc < 2) {
        kklog("Usage: open <filename>\n");
        return;
    }
    char* filename = argv[1];
    char* ext = strrchr(filename, '.');
    if (!ext) {
        kklog("Error: No file extension found.\n");
        return;
    }
    if (strcmp(ext, ".wav") == 0) {
        play_wav_file(filename);
    } 
    else if (strcmp(ext, ".txt") == 0) {
        cmd_read(argc, argv);
    } 
    else if (strcmp(ext, ".cos") == 0) {
    TplState *s = pmm_alloc_block();   // grab a 4KB block for state
    templar_init(s);
    templar_load_file(s, filename);
    pmm_free_block(s);
    }
    else {
        kklog("Error: Unsupported file type.\n");
    }
}

void cmd_playraw() {
    prep_play();
    select_drive(0x1F0, 1); 
    block_read(0, raw_wav_buffer);
    uint32_t* sig = (uint32_t*)raw_wav_buffer;
    if (sig[0] != 0x46464952) { 
        kklog("error wave not found");
        select_drive(0x1F0, 0);
        return;
    }
    kklog_green("Wave found\n");
    uint32_t current_lba = 0;
    for (int chunk = 0; chunk < 500; chunk++) {
        for (int i = 0; i < CHUNK_SECTORS; i++) {
            block_read(current_lba + i, raw_wav_buffer + (i * 512));
        }
        ac97_play_pcm(raw_wav_buffer, CHUNK_SECTORS * 512);
        current_lba += CHUNK_SECTORS;
    }
    kklog("finished\n");
    select_drive(0x1F0, 0); 
}

void playrawjmp() {
    create_task(cmd_playraw);
}

command_t commands[] = {
    {"help", cmd_help},
    {"clear", cmd_clear},
    {"reboot", cmd_reboot},
    {"cow", cmd_cow},
    {"cat", cmd_cat},
    {"ld", cmd_ld},
    {"fd", cmd_find},
    {"read", cmd_read},
    {"ls", cmd_ls},
    {"lib", cmd_lib},
    {"wr", cmd_wr},
    {"dl", cmd_dl},
    {"time", cmd_time},
    {"format", cmd_format},
    {"qformat", cmd_qformat},
    {"note", cmd_note},
    {"use", cmd_usedisk},
    {"mem", cmd_mem},
    {"play97", cmd_play97},
    {"shutdown", cmd_shutdown},
    {"app", cmd_app},
    {"open", cmd_open},
    //{"open", cmd_open},
    {"mf", cmd_mf},
    {"cd", cmd_cd},
    //{"open", cmd_open},
    {"play1", playrawjmp},
};

int command_count = sizeof(commands)/sizeof(command_t);