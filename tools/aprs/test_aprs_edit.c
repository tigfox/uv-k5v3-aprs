/* Host test for App/app/aprs_edit.c (arrow text entry) and App/app/aprs_items.c (choice items). */
#include <stdio.h>
#include <string.h>
#include "app/aprs_edit.h"
#include "app/aprs_items.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* spell a text with the arrows and STAR: each character is reached by stepping up from blank */
static void spell(aprs_edit_t *e, const char *text)
{
    for (const char *p = text; *p; p++) {
        for (int guard = 0; guard < 60 && e->buf[e->pos] != *p; guard++)
            APRS_EditArrow(e, +1);
        APRS_EditNext(e);
    }
}

static void test_call(void)
{
    aprs_edit_t e;
    aprs_settings_t s = APRS_SettingsDefaults();
    APRS_EditBegin(&e, APRS_EDIT_CALL);
    CHECK(strcmp(e.buf, "______") == 0);
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_UNCHANGED);            /* opened and committed: keeps N0CALL */
    CHECK(strcmp(s.call, "N0CALL") == 0);
    spell(&e, "W1AW");
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_OK && strcmp(s.call, "W1AW") == 0);
    CHECK(APRS_CallIsSet(&s));
    char v[40];
    APRS_EditView(&e, 6, v);
    CHECK(strcmp(v, "W1AW__\n    ^ ") == 0);

    /* digits can be typed directly and advance the cursor */
    APRS_EditBegin(&e, APRS_EDIT_CALL);
    spell(&e, "K");
    APRS_EditDigit(&e, 7);
    spell(&e, "XYZ");
    s = APRS_SettingsDefaults();
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_OK && strcmp(s.call, "K7XYZ") == 0);

    /* a gap inside the callsign is rejected, and the settings are left alone */
    APRS_EditBegin(&e, APRS_EDIT_CALL);
    spell(&e, "W");
    APRS_EditNext(&e);                                                  /* leave a blank */
    spell(&e, "A");
    s = APRS_SettingsDefaults();
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_INVALID && strcmp(s.call, "N0CALL") == 0);

    /* full length and no further */
    APRS_EditBegin(&e, APRS_EDIT_CALL);
    spell(&e, "ABCDEF");
    for (int i = 0; i < 5; i++) APRS_EditNext(&e);
    CHECK(e.pos == 5);
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_OK && strcmp(s.call, "ABCDEF") == 0);

    /* arrows wrap through the whole set in both directions */
    APRS_EditBegin(&e, APRS_EDIT_CALL);
    APRS_EditArrow(&e, -1);
    CHECK(e.buf[0] == '9');
    APRS_EditArrow(&e, +1);
    CHECK(e.buf[0] == '_');
}

static void test_back(void)
{
    aprs_edit_t e;
    APRS_EditBegin(&e, APRS_EDIT_CALL);
    CHECK(!APRS_EditBack(&e));                      /* empty: EXIT leaves the editor */
    spell(&e, "AB");                                /* cursor on the third position */
    CHECK(e.pos == 2 && APRS_EditBack(&e) && e.pos == 1 && e.buf[1] == '_');
    CHECK(APRS_EditBack(&e) && e.pos == 0 && e.buf[0] == '_');
    while (APRS_EditBack(&e)) { }
    CHECK(strcmp(e.buf, "______") == 0 && e.pos == 0);
}

static void test_loc(void)
{
    aprs_edit_t e;
    aprs_settings_t s = APRS_SettingsDefaults();
    APRS_EditBegin(&e, APRS_EDIT_LOC);
    CHECK(strcmp(e.buf, "000000000000000") == 0);
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_UNCHANGED);
    const char *code = "130712810599405";       /* 40.7128 N 74.0060 W */
    for (int i = 0; i < 15; i++) APRS_EditDigit(&e, (uint8_t)(code[i] - '0'));
    CHECK(e.pos == 14);
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_OK && strcmp(s.loc, code) == 0);
    /* a typo trips the checksum, and nothing is stored */
    s = APRS_SettingsDefaults();
    e.buf[3] = '9';
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_INVALID && s.loc[0] == 0);
    /* arrows change a digit with wrap-around */
    APRS_EditBegin(&e, APRS_EDIT_LOC);
    APRS_EditArrow(&e, -1);
    CHECK(e.buf[0] == '9');
    APRS_EditArrow(&e, +1);
    CHECK(e.buf[0] == '0');
    CHECK(APRS_EditBack(&e) == false);
}

static void test_comment_and_msgto(void)
{
    aprs_edit_t e;
    aprs_settings_t s = APRS_SettingsDefaults();
    APRS_EditBegin(&e, APRS_EDIT_CMNT);
    CHECK(e.max == 43);
    spell(&e, "RIDGE DIGI 7.5V");
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_OK && strcmp(s.comment, "RIDGE DIGI 7.5V") == 0);
    APRS_EditBegin(&e, APRS_EDIT_MSGTO);
    spell(&e, "N0CALL-7");
    CHECK(APRS_EditCommit(&e, &s) == APRS_EDIT_OK && strcmp(s.msgto, "N0CALL-7") == 0);
    char v[100];
    APRS_EditBegin(&e, APRS_EDIT_CMNT);
    for (int i = 0; i < 20; i++) APRS_EditNext(&e);
    APRS_EditView(&e, 8, v);                          /* the page holding position 20: 16..23 */
    CHECK(strlen(v) == 8 + 1 + 8 && v[9 + 4] == '^');
}

static void test_items(void)
{
    aprs_settings_t s = APRS_SettingsDefaults();
    CHECK(APRS_ItemIsChoice(APRS_MI_APRS) && APRS_ItemIsChoice(APRS_MI_INTV) && !APRS_ItemIsChoice(APRS_MI_CALL));
    CHECK(APRS_ItemMin(APRS_MI_DHOPS) == 1 && APRS_ItemMax(APRS_MI_DHOPS) == 7);
    CHECK(APRS_ItemMax(APRS_MI_SSID) == 15 && APRS_ItemMax(APRS_MI_DIGI) == 2 && APRS_ItemMax(APRS_MI_DDLY) == 3);
    s = APRS_ItemSet(&s, APRS_MI_DIGI, 2);
    CHECK(s.digi_mode == APRS_DIGI_WIDE && APRS_ItemGet(&s, APRS_MI_DIGI) == 2);
    s = APRS_ItemSet(&s, APRS_MI_DHOPS, 0);            /* clamped to 1 */
    CHECK(s.digi_hops == 1);
    s = APRS_ItemSet(&s, APRS_MI_DHOPS, 99);
    CHECK(s.digi_hops == 7);
    s = APRS_ItemSet(&s, APRS_MI_SSID, 9);
    CHECK(s.ssid == 9);
    s = APRS_ItemSet(&s, APRS_MI_INTV, 4);
    CHECK(s.interval_s == 600 && APRS_ItemGet(&s, APRS_MI_INTV) == 4);
    s = APRS_ItemSet(&s, APRS_MI_INTV, 0);
    CHECK(s.interval_s == 0 && APRS_ItemGet(&s, APRS_MI_INTV) == 0);
    s.interval_s = 700;                                  /* set by cable: the nearest choice below */
    CHECK(APRS_ItemGet(&s, APRS_MI_INTV) == 4);
    s = APRS_ItemSet(&s, APRS_MI_INTV, 99);
    CHECK(s.interval_s == 3600);
    const aprs_settings_t before = s;
    const aprs_settings_t same = APRS_ItemSet(&s, APRS_MI_CALL, 3);   /* not a choice: untouched */
    CHECK(memcmp(&before, &same, sizeof s) == 0);
    for (unsigned i = 0; i < APRS_MI_COUNT; i++) {                    /* every result is a valid record */
        if (!APRS_ItemIsChoice(i)) continue;
        for (int32_t v = APRS_ItemMin(i); v <= APRS_ItemMax(i); v++) {
            aprs_settings_t n = APRS_ItemSet(&s, i, v);
            CHECK(APRS_SettingsValid(&n) && APRS_ItemGet(&n, i) == v);
        }
    }
}

int main(void)
{
    test_call(); test_back(); test_loc(); test_comment_and_msgto(); test_items();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
