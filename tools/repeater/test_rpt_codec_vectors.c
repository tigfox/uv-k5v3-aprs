/* Prints RPT_FreqCheck for a list of frequencies and decodes table images: the C side of the Python/C pin. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app/rpt_info.h"

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "checks")) {
        static const uint32_t f[] = { 0, 1, 14682000u, 14694000u, 14400000u, 44350000u, 14639000u, 0xFFFFFFFFu, 12345678u };
        for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++)
            printf("%u %u\n", f[i], RPT_FreqCheck(f[i]));
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "decode")) {          /* decode <image> : <channel> <freq10> pairs on stdin */
        FILE *img = fopen(argv[2], "rb");
        if (!img) return 2;
        static uint8_t table[RPT_INFO_SLOTS * RPT_INFO_RECORD];
        if (fread(table, 1, sizeof table, img) != sizeof table) return 3;
        fclose(img);
        unsigned ch; unsigned long freq;
        while (scanf("%u %lu", &ch, &freq) == 2) {
            char out[RPT_INFO_TEXT_MAX + 1];
            const bool ok = ch < RPT_INFO_SLOTS && RPT_InfoDecode(table + ch * RPT_INFO_RECORD, (uint32_t)freq, out);
            printf("%u|%s\n", ch, ok ? out : "");
        }
        return 0;
    }
    return 1;
}
