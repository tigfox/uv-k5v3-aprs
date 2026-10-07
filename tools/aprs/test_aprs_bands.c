/* Host test for App/app/aprs_bands.c (frequencies are in units of 10 Hz). */
#include "app/aprs_bands.h"
#include <stdio.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void)
{
    /* 2 m: 144.000-148.000 MHz, 70 cm: 420.000-450.000 MHz; both ends inclusive of the lower edge only */
    CHECK(APRS_FreqAllowed(14400000));
    CHECK(APRS_FreqAllowed(14439000));   /* 144.390 APRS */
    CHECK(APRS_FreqAllowed(14799999));
    CHECK(!APRS_FreqAllowed(14800000));
    CHECK(!APRS_FreqAllowed(14399999));
    CHECK(APRS_FreqAllowed(42000000));
    CHECK(APRS_FreqAllowed(44499999));
    CHECK(!APRS_FreqAllowed(45000000));
    CHECK(!APRS_FreqAllowed(41999999));
    CHECK(!APRS_FreqAllowed(0));
    CHECK(!APRS_FreqAllowed(16000000));  /* between the bands */
    CHECK(!APRS_FreqAllowed(15500000));  /* MURS */
    CHECK(!APRS_FreqAllowed(0xFFFFFFFFu));
    CHECK(APRS_FreqClamp(10000000) == 14400000);   /* below 2 m -> 2 m low edge */
    CHECK(APRS_FreqClamp(14600000) == 14600000);
    CHECK(APRS_FreqClamp(16000000) == 14400000);   /* gap -> 2 m low edge */
    CHECK(APRS_FreqClamp(43000000) == 43000000);
    CHECK(APRS_FreqClamp(50000000) == 14400000);
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
