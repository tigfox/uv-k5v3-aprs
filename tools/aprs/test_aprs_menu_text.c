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
    is(&s, APRS_MI_CMNT, "A very l\nong comm");   /* 2 lines of 8, rest elided */
    is(&s, 99, "N/A");
    { char tiny[4]; APRS_MenuText(&s, APRS_MI_CALL, tiny, sizeof tiny); CHECK(strlen(tiny) == 3); }
    { char one[1] = {'x'}; APRS_MenuText(&s, APRS_MI_CALL, one, 1); CHECK(one[0] == 0); }
    {
        aprs_dstat_t d = { 12, 0, 21, 48, "W1ABC-7", true };
        char b[24];
        APRS_DStatText(&d, 0, b, sizeof b); CHECK(strcmp(b, "HRD 12\nW1ABC-7") == 0);
        APRS_DStatText(&d, 1, b, sizeof b); CHECK(strcmp(b, "HRD 12\navg 21us") == 0);
        APRS_DStatText(&d, 2, b, sizeof b); CHECK(strcmp(b, "HRD 12\nmax 48us") == 0);
        APRS_DStatText(&d, 3, b, sizeof b); CHECK(strcmp(b, "HRD 12\nW1ABC-7") == 0);
        d.last[0] = 0; APRS_DStatText(&d, 0, b, sizeof b); CHECK(strcmp(b, "HRD 12\n--") == 0);
        d.running = false; APRS_DStatText(&d, 0, b, sizeof b); CHECK(strcmp(b, "OFF") == 0);
        d.running = true; APRS_DStatText(&d, 1, b, 5); CHECK(strlen(b) == 4);
    }
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
