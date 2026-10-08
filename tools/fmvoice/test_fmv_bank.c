/* Host test for App/app/fmv_bank.c: moving through a bank, landing on a bank, bank labels. */
#include <stdio.h>
#include <string.h>
#include "app/fmv_bank.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_STR(got, want) do { checks++; if (strcmp((got), (want))) { fails++; \
    printf("FAIL %s:%d: got '%s' want '%s'\n", __FILE__, __LINE__, (got), (want)); } } while (0)

#define TOTAL 20u
static bool in_bank(uint16_t ch, void *ctx)
{
    const uint32_t mask = *(const uint32_t *)ctx;      /* bit i: channel i is in the bank */
    return ch < 32u && ((mask >> ch) & 1u);
}

static void test_next(void)
{
    uint32_t bank = (1u << 3) | (1u << 7) | (1u << 8) | (1u << 19);
    CHECK(FMV_NextIn(0, 1, TOTAL, in_bank, &bank) == 3);
    CHECK(FMV_NextIn(3, 1, TOTAL, in_bank, &bank) == 3);          /* inclusive start */
    CHECK(FMV_NextIn(4, 1, TOTAL, in_bank, &bank) == 7);
    CHECK(FMV_NextIn(9, 1, TOTAL, in_bank, &bank) == 19);
    CHECK(FMV_NextIn(20, 1, TOTAL, in_bank, &bank) == 3);         /* past the end wraps to the start */
    CHECK(FMV_NextIn(8, -1, TOTAL, in_bank, &bank) == 8);
    CHECK(FMV_NextIn(6, -1, TOTAL, in_bank, &bank) == 3);
    CHECK(FMV_NextIn(2, -1, TOTAL, in_bank, &bank) == 19);        /* before the start wraps to the end */
    CHECK(FMV_NextIn(0xFFFF, -1, TOTAL, in_bank, &bank) == 19);   /* what "channel 0 minus one" gives */
    uint32_t zero = (1u << 0) | (1u << 5);                        /* channel 0 is in the bank: DOWN from 0 wraps */
    CHECK(FMV_NextIn(0xFFFF, -1, TOTAL, in_bank, &zero) == 5);
    CHECK(FMV_NextIn(TOTAL, 1, TOTAL, in_bank, &zero) == 0);
    uint32_t one = 1u << 5;
    CHECK(FMV_NextIn(0, 1, TOTAL, in_bank, &one) == 5 && FMV_NextIn(6, 1, TOTAL, in_bank, &one) == 5);
    CHECK(FMV_NextIn(5, -1, TOTAL, in_bank, &one) == 5);
    uint32_t none = 0;
    CHECK(FMV_NextIn(0, 1, TOTAL, in_bank, &none) == 0xFFFF && FMV_NextIn(7, -1, TOTAL, in_bank, &none) == 0xFFFF);
    CHECK(FMV_NextIn(0, 1, 0, in_bank, &bank) == 0xFFFF);          /* no channels at all */
    CHECK(FMV_NextIn(0, 0, TOTAL, in_bank, &bank) == 0xFFFF);      /* not a direction */
    /* a full lap visits each channel of the bank once */
    unsigned seen = 0;
    uint16_t ch = FMV_NextIn(0, 1, TOTAL, in_bank, &bank);
    for (int i = 0; i < 10 && seen < 4; i++, seen++)
        ch = FMV_NextIn((uint16_t)(ch + 1), 1, TOTAL, in_bank, &bank);
    CHECK(seen == 4 && ch == 3);
}

static void test_target(void)
{
    uint32_t bank = (1u << 3) | (1u << 7) | (1u << 19);
    CHECK(FMV_BankTarget(7, TOTAL, in_bank, &bank) == 7);                         /* back where it was left */
    CHECK(FMV_BankTarget(FMV_BANK_LAST_NONE, TOTAL, in_bank, &bank) == 3);        /* never visited: the first channel */
    CHECK(FMV_BankTarget(8, TOTAL, in_bank, &bank) == 3);                         /* the channel left the bank */
    CHECK(FMV_BankTarget(500, TOTAL, in_bank, &bank) == 3);                       /* out of range */
    uint32_t none = 0;
    CHECK(FMV_BankTarget(7, TOTAL, in_bank, &none) == 0xFFFF);                    /* an empty bank */
}

static void test_label(void)
{
    char o[16];
    FMV_BankLabel(3, "HOME", 25, 26, o); CHECK_STR(o, "BANK HOME");
    FMV_BankLabel(3, "ME  ", 25, 26, o); CHECK_STR(o, "BANK ME");
    FMV_BankLabel(7, "    ", 25, 26, o); CHECK_STR(o, "BANK 07");                  /* blank name: the number */
    FMV_BankLabel(24, "\xFF\xFF\xFF\xFF", 25, 26, o); CHECK_STR(o, "BANK 24");     /* erased name */
    FMV_BankLabel(1, "\x01" "AB", 25, 26, o); CHECK_STR(o, "BANK 01");
    FMV_BankLabel(1, "", 25, 26, o); CHECK_STR(o, "BANK 01");
    FMV_BankLabel(25, "ZZZZ", 25, 26, o); CHECK_STR(o, "BANK ALL");
    FMV_BankLabel(26, "ZZZZ", 25, 26, o); CHECK_STR(o, "BANK MIX");
    FMV_BankLabel(12, "ABCD", 25, 26, o); CHECK_STR(o, "BANK ABCD"); CHECK(strlen(o) <= 11);
}

int main(void)
{
    test_next(); test_target(); test_label();
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
