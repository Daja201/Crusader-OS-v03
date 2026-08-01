bits 32
global loader
extern kmain

; VARIABLES:

MAGIC_NUMBER      equ 0x1BADB002
FLAGS             equ (1<<0) | (1<<1) | (1<<2)   ; Bit 2 is REQUIRED for video mode
CHECKSUM          equ -(MAGIC_NUMBER + FLAGS)
KERNEL_STACK_SIZE equ 65536

;MAIN:

section .multiboot
    align 4
    dd MAGIC_NUMBER
    dd FLAGS
    dd CHECKSUM
    
    ; Address kludge fields (5 of them, required by GRUB if Bit 2 is set)
    dd 0
    dd 0
    dd 0
    dd 0
    dd 0 
    
; Video mode setup
    dd 0    
    dd 1280  ; Width
    dd 720  ; Height
    dd 32   ; Depth

section .bss
    align 16
kernel_stack:
    resb KERNEL_STACK_SIZE

section .text
    align 4

loader:
    lea esp, [kernel_stack + KERNEL_STACK_SIZE]
    mov ebp, esp

    ; call kmain(mb_magic, mb_info)
    push ebx    ; mb_info
    push eax    ; mb_magic
    call kmain
    add esp, 8

    ;kills cpu if kmain comes back lol

.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack