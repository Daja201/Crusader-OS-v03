#include "library.h"
#include "fs.h"
#include "klog.h"
void library(void){
    vesa_clear(0X000000);
    //
    kklog("This is an simple operating system made by David Zapletal. Github: Daja201");
    kklog("Here is simple library for using this operating system.");
    //
    kklog("  BASIC:");
    kklog("help - some basic info");
    kklog("clear -  clears display");
    kklog("cow - makes an cow jump through your window");
    kklog("cat - prints cute cat");
    kklog("reboot - reboots using triple fault");
    kklog("lib - bro u are using lib rn you should know what it does");
    //
    kklog("  FS:");
    kklog("ld - detects and checks drives");
    kklog("read - reads from some files");
    kklog("ls - reads all files from directory");
    kklog("dl - deletes file, use dl <file>");
    kklog("wr - writes into file, use wr <file> <content>");
    //
    kklog("  ADVANCED:");
    kklog("rt - runs runtest.c and make some runtests");
    kklog("time - shows time from RealTimeClock");
    //
}
