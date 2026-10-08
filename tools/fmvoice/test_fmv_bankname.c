/* Host test for the long bank names in App/app/fmv_bank.c: cleaning the 16 stored bytes, the label shown on the card. */
#include <stdio.h>
#include <string.h>
#include "app/fmv_bank.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(got, want) do { checks++; if (strcmp((got), (want))) { fails++; \
    printf("FAIL %s:%d: got '%s' want '%s'\n", __FILE__, __LINE__, (got), (want)); } } while (0)

static bool clean(const char *raw16, char *out)
{
    return FMV_BankNameClean((const uint8_t *)raw16, out);
}

static void test_clean(void)
{
    char out[FMV_BANKNAME_MAX + 1];
    CHECK(clean("GMRS Repeaters  ", out)); CHECK_STR(out, "GMRS Repeaters");           /* padding spaces trimmed */
    CHECK(clean("0123456789ABCDEF", out)); CHECK_STR(out, "0123456789ABCDEF");         /* all 16, no terminator */
    CHECK(clean("WX\0\0\0\0\0\0\0\0\0\0\0\0\0\0", out)); CHECK_STR(out, "WX");          /* NUL padding */
    CHECK(clean("FRS\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", out)); CHECK_STR(out, "FRS");   /* erased flash padding */
    CHECK(!clean("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", out)); CHECK_STR(out, "");   /* never written */
    CHECK(!clean("                ", out)); CHECK_STR(out, "");                         /* only spaces */
    CHECK(!clean("\0               ", out)); CHECK_STR(out, "");
    CHECK(!clean("GM\x01RS           ", out)); CHECK_STR(out, "");                      /* a control byte: the whole name is junk */
    CHECK(!clean("GM\x80RS           ", out)); CHECK_STR(out, "");                      /* a byte above ASCII */
    CHECK(clean("A  B            ", out)); CHECK_STR(out, "A  B");                      /* inner spaces kept */
    CHECK(clean(" LEAD           ", out)); CHECK_STR(out, " LEAD");                     /* leading space kept (not trimmed) */
}

static void test_label(void)
{
    char out[FMV_BANKLABEL_MAX];
    FMV_BankLabelLong(25, "ignored", "ALL ", 25, 26, out); CHECK_STR(out, "BANK ALL");
    FMV_BankLabelLong(26, "ignored", "MIX ", 25, 26, out); CHECK_STR(out, "BANK MIX");
    FMV_BankLabelLong(3, "GMRS Repeaters", "GMR ", 25, 26, out); CHECK_STR(out, "GMRS Repeaters");   /* the long name alone: it fills the row */
    FMV_BankLabelLong(3, "", "GMR ", 25, 26, out); CHECK_STR(out, "BANK GMR");                      /* no long name: the short one */
    FMV_BankLabelLong(3, "", "\xff\xff\xff\xff", 25, 26, out); CHECK_STR(out, "BANK 03");           /* neither: the number */
    FMV_BankLabelLong(3, "0123456789ABCDEF", "", 25, 26, out); CHECK(strlen(out) == 16);
}

static void test_wrap(void)
{
    char a[FMV_BANKNAME_MAX + 1], b[FMV_BANKNAME_MAX + 1];
    FMV_BankNameWrap("GMRS", 11, a, b); CHECK_STR(a, "GMRS"); CHECK_STR(b, "");                       /* fits: one line */
    FMV_BankNameWrap("ElevenChars", 11, a, b); CHECK_STR(a, "ElevenChars"); CHECK_STR(b, "");
    FMV_BankNameWrap("GMRS Repeaters", 11, a, b); CHECK_STR(a, "GMRS"); CHECK_STR(b, "Repeaters");    /* breaks at the space */
    FMV_BankNameWrap("Local Area Simplex", 11, a, b); CHECK_STR(a, "Local Area"); CHECK_STR(b, "Simplex");   /* the last space that fits */
    FMV_BankNameWrap("0123456789ABCDEF", 11, a, b); CHECK_STR(a, "0123456789A"); CHECK_STR(b, "BCDEF");    /* no space: hard split */
    FMV_BankNameWrap("A 0123456789ABCD", 11, a, b); CHECK_STR(a, "A"); CHECK_STR(b, "0123456789A");       /* the rest is cut to the width */
    FMV_BankNameWrap("", 11, a, b); CHECK_STR(a, ""); CHECK_STR(b, "");
}

int main(void)
{
    test_clean();
    test_label();
    test_wrap();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
