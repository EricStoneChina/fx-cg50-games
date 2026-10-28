#ifndef CG50_GAMES_MAIN_MENU_H
#define CG50_GAMES_MAIN_MENU_H

static void openMainMenu(void) {
    /* Find the OS routine that saves and restores the add-in's own context. */
    unsigned int table = *(volatile unsigned int *)0x8002007c;
    if ((table & 3) || table < 0x80020070 || table > 0x81000000 - 0x1e59 * 4) return;
    unsigned int entry = *(const unsigned int *)(table + 0x1e58 * 4);
    if ((entry & 1) || entry < 0x80020070 || entry > 0x81000000 - 300) return;
    const unsigned short *code = (const unsigned short *)entry;
    for (unsigned int i = 0; i + 5 < 150; i++) {
        if ((code[i] & 0xff00) != 0xde00 || code[i + 1] != 0xe203 ||
            (code[i + 2] & 0xf000) != 0xb000 || code[i + 3] != 0x2e20 ||
            (code[i + 4] & 0xf000) != 0xa000 || code[i + 5] != 0x0009) continue;
        int displacement = code[i + 2] & 0x0fff;
        if (displacement & 0x0800) displacement -= 0x1000;
        unsigned int target = entry + i * 2 + 8 + displacement * 2;
        if (target < 0x80020070 || target >= 0x81000000) return;
        ((void (*)(void))target)();
        return;
    }
}

#endif
