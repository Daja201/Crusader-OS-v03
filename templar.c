//interpreted programming language for COS
#include "fs.h"

void templar_file(filename) {
    inode_t root;
    inode_t file_node;
    int inode_num = dir_lookup(&root, filename);
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
}
