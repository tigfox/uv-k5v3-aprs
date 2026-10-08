/* Host test for App/app/aprs_menu_text.c */
#include "app/aprs_menu_text.h"
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static void is(const aprs_settings_t *s, unsigned item, const char *want)
{
    char buf[24];
    APRS_MenuText(s, item, buf, sizeof buf);
    checks++;
    if (strcmp(buf, want)) { fails++; printf("FAIL item %u: got '%s' want '%s'\n", item, buf, want); }
}

int main(void)
{
    aprs_settings_t s = APRS_SettingsDefaults();
    is(&s, APRS_MI_APRS, "OFF");
    is(&s, APRS_MI_DIGI, "OFF");
    is(&s, APRS_MI_DHOPS, "WIDE2-2");
    is(&s, APRS_MI_DDLY, "OFF");
    is(&s, APRS_MI_BCNTY, "MOBILE");
    is(&s, APRS_MI_DSTAT, "--");
    is(&s, APRS_MI_INTV, "10min");
    is(&s, APRS_MI_CALL, "N0CALL");
    is(&s, APRS_MI_SSID, "0");
    is(&s, APRS_MI_LOC, "NONE");
    is(&s, APRS_MI_CMNT, "");
    is(&s, APRS_MI_MSGTO, "");
    is(&s, APRS_MI_SEND, "N/A");
    s.aprs_on = 1; s.digi_mode = APRS_DIGI_WIDE; s.digi_hops = 3; s.digi_delay = 3; s.beacon_type = 1;
    s.interval_s = 90; s.ssid = 15;
    strcpy(s.call, "W1ABC"); strcpy(s.loc, "123456789012345");
    strcpy(s.comment, "A very long comment indeed that overflows");
    is(&s, APRS_MI_APRS, "ON");
    is(&s, APRS_MI_DIGI, "WIDE");
    is(&s, APRS_MI_DHOPS, "WIDE3-3");
    is(&s, APRS_MI_DDLY, "1s");
    is(&s, APRS_MI_BCNTY, "DIGI");
    is(&s, APRS_MI_INTV, "90s");
    is(&s, APRS_MI_SSID, "15");
    is(&s, APRS_MI_LOC, "12345678\n9012345");
    is(&s, APRS_MI_CMNT, "A very l\nong comm");
    strcpy(s.loc, "130712810599405");
    is(&s, APRS_MI_LOC, "40.71N\n74.00W");
    strcpy(s.loc, "123456789012345");   /* 2 lines of 8, rest elided */
    is(&s, 99, "N/A");
    { char tiny[4]; APRS_MenuText(&s, APRS_MI_CALL, tiny, sizeof tiny); CHECK(strlen(tiny) == 3); }
    { char one[1] = {'x'}; APRS_MenuText(&s, APRS_MI_CALL, one, 1); CHECK(one[0] == 0); }
    {
        char l[APRS_BOX_ROWS][APRS_BOX_COLS + 1];
        CHECK(APRS_BoxLines("", l) == 0);
        CHECK(APRS_BoxLines("short", l) == 1 && strcmp(l[0], "short") == 0);
        CHECK(APRS_BoxLines("0123456789ABCDEF", l) == 1 && strlen(l[0]) == 16);               /* exactly one row */
        CHECK(APRS_BoxLines("0123456789ABCDEFG", l) == 2 && strcmp(l[1], "G") == 0);
        CHECK(APRS_BoxLines("N0CALL-3>meet at the ridge at noon", l) == 3 && strcmp(l[2], "on") == 0);
        const char *longtxt = "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEFXXXXXXXX";
        CHECK(APRS_BoxLines(longtxt, l) == 3 && strlen(l[2]) == 16 && strchr(l[2], 'X') == NULL);   /* cut at 48 */
    }
    {
        char b[24];
        APRS_MenuTextLines("N0CALL-3", b, sizeof b); CHECK(strcmp(b, "N0CALL-3") == 0);
        APRS_MenuTextLines("K1ABC-12 EXTRA", b, sizeof b); CHECK(strcmp(b, "K1ABC-12\n EXTRA") == 0);
        const char *m = "N0CALL-3>meet at the ridge";
        APRS_MenuMsgPage(m, 0, b, sizeof b); CHECK(strcmp(b, "N0CALL-3\n>meet at") == 0);
        APRS_MenuMsgPage(m, 1, b, sizeof b); CHECK(strcmp(b, " the rid\nge") == 0);
        APRS_MenuMsgPage(m, 2, b, sizeof b); CHECK(b[0] == 0);
    }
    {
        aprs_dstat_t d = { .heard = 12, .repeated = 5, .dup = 3, .cancelled = 2, .toomany = 1, .digi_dropped = 4,
                           .dropped = 1, .isr_avg_us = 21, .isr_max_us = 48, .last = "W1ABC-7", .running = true };
        char b[24];
        const char *want[APRS_DSTAT_VIEWS] = { "HRD 12\nW1ABC-7", "HRD 12\nRPT 5", "HRD 12\nDUP 3", "HRD 12\nCNL 2",
                                               "HRD 12\nHOP 1", "HRD 12\nDRP 5", "HRD 12\navg 21us", "HRD 12\nmax 48us" };
        for (unsigned i = 0; i < APRS_DSTAT_VIEWS; i++) {
            APRS_DStatText(&d, i, b, sizeof b); CHECK(strcmp(b, want[i]) == 0);
            APRS_DStatText(&d, i + APRS_DSTAT_VIEWS, b, sizeof b); CHECK(strcmp(b, want[i]) == 0);   /* it wraps */
        }
        d.last[0] = 0; APRS_DStatText(&d, 0, b, sizeof b); CHECK(strcmp(b, "HRD 12\n--") == 0);
        d.running = false; APRS_DStatText(&d, 0, b, sizeof b); CHECK(strcmp(b, "OFF") == 0);
        d.running = true; APRS_DStatText(&d, 1, b, 5); CHECK(strlen(b) == 4);
    }
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
