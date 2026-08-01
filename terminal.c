#include "terminal.h"
#include "commands.h"
#include "klog.h"
#include <string.h>

char cmd_buf[CMD_BUF_SIZE];
int cmd_len = 0;
extern char g_current_path[64] = "";

void execute_command(char* line) {
    char* argv[8];
    int argc = 0;
    char* p = line;
    if (line == NULL || line[0] == '\0') {
        return;
    }
    while (*p && argc < 8) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    for (int i = 0; i < command_count; i++) { 
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].func(argc, argv);
            return;
        }
    }
    klog("sorry buddy we don't know this one\n");
}

void terminal_key(char c) {
    if (c == 0) return;
    if (c == '\n') {
        cmd_buf[cmd_len] = 0;
        klog("\n");
        execute_command(cmd_buf);
        cmd_len = 0;
        if (c == '\n') {
        cmd_buf[cmd_len] = 0;
        klog("\n");
        execute_command(cmd_buf);
        cmd_len = 0;
        klog_yellow("CRUSADER");
        klog_orange(g_current_path);
        klog_yellow(">> ");
        return;
    }
        return;
    }
    if (c == '\b') {
        if (cmd_len > 0) {
            cmd_len--;
            klog("\b");
        }
        return;
    }
    if (cmd_len < CMD_BUF_SIZE - 1) {
        cmd_buf[cmd_len++] = c;
        char str[2] = {c, '\0'};
        klog(str); 
    }
    if (c_y > 720) {
        vesa_draw_rec(0, 0, 100, 100, 0xFF0000);
        vesa_swap();
    }
}