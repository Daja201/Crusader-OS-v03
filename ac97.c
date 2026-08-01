#include "ac97.h"
#include "pci.h"
#include "klog.h"
#include "io.h"
#include "formats.h"
#include "fs.h"
#include "ac97.h"
#include "klog.h"
#include <string.h>
#include <stdint.h>

#define CHUNK_SIZE 65532
#define CHUNK_SECTORS 256
extern void select_drive(uint16_t base, uint8_t slave);
extern void block_read(uint32_t lba, uint8_t* buf);
static uint8_t raw_wav_buffer[CHUNK_SECTORS * 512] __attribute__((aligned(8)));

extern uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
extern void pci_config_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
pci_device_t g_dev;
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

int ac97_init(void) {
    if (pci_find_class(0x04, 0x01, &g_dev, 0) != 0) {
        kklog("NO AC97 DEVICE");
        return -1;
    }
    return 0;
}

int ac97_play_pcm(void* buffer, uint32_t length_bytes) {
    uint32_t real_bar1 = pci_config_read(g_dev.bus, g_dev.device, g_dev.function, 0x14);
    uint16_t nabm_port = real_bar1 & ~0x3; 
    outb(nabm_port + 0x1B, 0x02); 
    bdl[0].buffer_addr = (uint32_t)buffer;
    bdl[0].length = length_bytes / 2; 
    bdl[0].flags = 0x8000;
    outl(nabm_port + 0x10, (uint32_t)&bdl);
    outb(nabm_port + 0x15, 0); 
    outb(nabm_port + 0x1B, 0x01);
    while (!(inw(nabm_port + 0x16) & 0x1C)) {
        asm volatile("pause");
    }
    outw(nabm_port + 0x16, 0x1C);
    outb(nabm_port + 0x1B, 0x00);
    return 0;
}

int prep_play(void){
    uint32_t cmd = pci_config_read(g_dev.bus, g_dev.device, g_dev.function, 0x04);
    pci_config_write(g_dev.bus, g_dev.device, g_dev.function, 0x04, cmd | 0x05);
}

int ac97_play_test_tone(void) {
    if (g_dev.vendor_id == 0) {
        kklog("NO AC97 DEVICE");
        return -1;
    }
    uint32_t cmd = pci_config_read(g_dev.bus, g_dev.device, g_dev.function, 0x04);
    pci_config_write(g_dev.bus, g_dev.device, g_dev.function, 0x04, cmd | 0x05);
    uint32_t real_bar0 = pci_config_read(g_dev.bus, g_dev.device, g_dev.function, 0x10);
    uint32_t real_bar1 = pci_config_read(g_dev.bus, g_dev.device, g_dev.function, 0x14);
    uint16_t nam_port = real_bar0 & ~0x3;  
    uint16_t nabm_port = real_bar1 & ~0x3; 
    if (nam_port == 0 || nabm_port == 0) {
        return -1;
    }
    outw(nam_port + 0x00, 1);
    outw(nam_port + 0x02, 0x0000); 
    outw(nam_port + 0x18, 0x0000);
    int period = 54; 
    int16_t volume = 2000; 
    for (int i = 0; i < 32000; i += 2) {
        if ((i / 2) % period < (period / 2)) {
            audio_buffer[i] = volume; audio_buffer[i+1] = volume;
        } else {
            audio_buffer[i] = -volume; audio_buffer[i+1] = -volume;
        }
    }
    outb(nabm_port + 0x1B, 0x02);
    bdl[0].buffer_addr = (uint32_t)&audio_buffer;
    bdl[0].length = 16000;
    bdl[0].flags = 0x8000;
    outl(nabm_port + 0x10, (uint32_t)&bdl);            
    outb(nabm_port + 0x15, 0);                
    outb(nabm_port + 0x1B, 0x01);      
    while ((inw(nabm_port + 0x16) & 0x08) == 0) {
        asm volatile("pause"); 
    }
    outw(nabm_port + 0x16, 0x08);
    outb(nabm_port + 0x1B, 0x00);        
    kklog("BEEP");
    return 0;
}

int play_wav_file(const char* filename) {
    int inode_num = fs_resolve_path(filename, g_current_dir);
    if (inode_num < 0) {
        kklogf_green("WAV: %s not found\n", filename);
        return -1;
    }
    inode_t file_node;
    read_inode(inode_num, &file_node);
    wav_header_t header;
    fs_read((uint32_t)inode_num, &file_node, 0, sizeof(wav_header_t), (uint8_t*)&header);
    if (header.chunkId[0] != 'R' || header.format[0] != 'W') {
        kklog("WAV: Invalid format\n");
        return -1;
    }
    uint32_t total_data = header.subchunk2Size;
    uint32_t offset = sizeof(wav_header_t);
    uint32_t played = 0;
    kklogf_green("Playing: %s (%u bytes)\n", filename, total_data);
    while (played < total_data) {
        uint32_t to_read = total_data - played;
        if (to_read > CHUNK_SIZE) to_read = CHUNK_SIZE;
        fs_read((uint32_t)inode_num, &file_node, offset, to_read, streaming_buffer);
        ac97_play_pcm(streaming_buffer, to_read);
        played += to_read;
        offset += to_read;
    } 
    kklog("WAV: Finished.\n");
    return 0;
}
