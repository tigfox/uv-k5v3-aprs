/* Host test for App/app/fmv_tone.c: the text shown when a tone search finishes. */
#include <stdio.h>
#include <string.h>
#include "app/fmv_tone.h"

static int fails, checks;
#define CHECK_STR(got, want) do { checks++; if (strcmp((got), (want))) { fails++; \
    printf("FAIL %s:%d: got '%s' want '%s'\n", __FILE__, __LINE__, (got), (want)); } } while (0)

int main(void)
{
    char o[FMV_TONE_LABEL_MAX];
    FMV_ToneLabel(false, 1000, o); CHECK_STR(o, "TX PL 100.0");
    FMV_ToneLabel(false, 670, o);  CHECK_STR(o, "TX PL 67.0");
    FMV_ToneLabel(false, 2541, o); CHECK_STR(o, "TX PL 254.1");
    FMV_ToneLabel(false, 885, o);  CHECK_STR(o, "TX PL 88.5");
    FMV_ToneLabel(false, 0, o);    CHECK_STR(o, "TX PL 0.0");
    FMV_ToneLabel(false, 9999, o); CHECK_STR(o, "TX PL 999.9");
    FMV_ToneLabel(true, 023, o);   CHECK_STR(o, "TX DCS 023");     /* the code is held as the number 023 octal */
    FMV_ToneLabel(true, 0754, o);  CHECK_STR(o, "TX DCS 754");
    FMV_ToneLabel(true, 0, o);     CHECK_STR(o, "TX DCS 000");
    FMV_ToneLabel(true, 0xFFFF, o); CHECK_STR(o, "TX DCS ???");    /* not a code: never print garbage */
    FMV_ToneLabel(false, 0xFFFF, o); CHECK_STR(o, "TX PL ???");
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
