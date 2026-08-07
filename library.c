#include "library.h"
#include "fs.h"
#include "klog.h"
#include "string.h"

typedef struct {
    const char* name;
    const char* section;
    const char* usage;
    const char* desc;
} lib_entry_t;

static const lib_entry_t lib_entries[] = {
    // BASIC
    {"help",     "BASIC", "help",                    "some basic info"},
    {"clear",    "BASIC", "clear",                   "clears display"},
    {"cow",      "BASIC", "cow",                      "makes an cow jump through your window"},
    {"cat",      "BASIC", "cat",                      "prints cute cat"},
    {"reboot",   "BASIC", "reboot",                   "reboots using triple fault"},
    {"shutdown", "BASIC", "shutdown",                 "shuts down the system"},
    {"mem",      "BASIC", "mem",                      "shows free memory"},
    {"lib",      "BASIC", "lib [command]",            "bro u are using lib rn you should know what it does"},
    // FS
    {"ld",       "FS", "ld",                          "detects and checks drives"},
    {"use",      "FS", "use <drive_index> | use <port> <0|1>", "switches active drive (port: 0=primary,1=secondary or 0x1F0/0x170)"},
    {"fs",       "FS", "fs <custom|fat32>",           "shows/switches active filesystem"},
    {"read",     "FS", "read <file>",                 "reads from some files"},
    {"ls",       "FS", "ls",                          "reads all files from directory"},
    {"cd",       "FS", "cd <dir>",                    "changes current directory"},
    {"mf",       "FS", "mf <name>",                   "makes a new directory"},
    {"dl",       "FS", "dl <file>",                   "deletes file"},
    {"wr",       "FS", "wr <file> <content>",         "writes into file"},
    {"fd",       "FS", "fd <tag>",                    "finds files by tag"},
    {"format",   "FS", "format [fat32]",               "formats current drive"},
    // FAT32
    {"format32", "FAT32", "format32",                 "formats current drive as FAT32"},
    {"mount32",  "FAT32", "mount32",                  "mounts a FAT32 volume"},
    {"ls32",     "FAT32", "ls32",                      "lists files on FAT32 volume"},
    {"cd32",     "FAT32", "cd32 <dir>",                "changes directory on FAT32 volume"},
    {"read32",   "FAT32", "read32 <file>",             "reads a file from FAT32 volume"},
    {"wr32",     "FAT32", "wr32 <file> <content>",     "writes into a file on FAT32 volume"},
    {"dl32",     "FAT32", "dl32 <file>",               "deletes a file on FAT32 volume"},
    {"mf32",     "FAT32", "mf32 <name>",               "makes a directory on FAT32 volume"},
    {"ap32",     "FAT32", "ap32 <file> <content>",     "appends into a file on FAT32 volume"},
    {"qformat",  "FAT32", "qformat",                   "quick formats current drive with custom fs"},
    // ADVANCED
    {"time",     "ADVANCED", "time",                  "shows time from RealTimeClock"},
    {"app",      "ADVANCED", "app <app_name>",         "runs an app"},
    {"play97",   "ADVANCED", "play97",                 "plays a test tone through AC97"},
    {"play1",    "ADVANCED", "play1",                  "plays raw wav from disk"},
};

static const int lib_entry_count = sizeof(lib_entries) / sizeof(lib_entry_t);

static void library_print_all(void) {
    vesa_clear(0X000000);
    //
    kklog("This is an simple operating system made by David Zapletal. Github: Daja201");
    kklog("Here is simple library for using this operating system.");
    kklog("Run 'lib <command>' for detailed info about a specific command.");
    //
    const char* current_section = "";
    for (int i = 0; i < lib_entry_count; i++) {
        if (strcmp(current_section, lib_entries[i].section) != 0) {
            current_section = lib_entries[i].section;
            klogf("  %s:\n", current_section);
        }
        klogf("%s - %s\n", lib_entries[i].name, lib_entries[i].desc);
    }
}

static void library_print_entry(const char* name) {
    for (int i = 0; i < lib_entry_count; i++) {
        if (strcmp(lib_entries[i].name, name) == 0) {
            klogf("NAME:\n    %s\n", lib_entries[i].name);
            klogf("SECTION:\n    %s\n", lib_entries[i].section);
            klogf("USAGE:\n    %s\n", lib_entries[i].usage);
            klogf("DESCRIPTION:\n    %s\n", lib_entries[i].desc);
            return;
        }
    }
    klogf("No manual entry for '%s'\n", name);
    kklog("Run 'lib' with no arguments to see all commands.");
}

void library(int argc, char** argv) {
    if (argc < 2) {
        library_print_all();
        return;
    }
    library_print_entry(argv[1]);
}