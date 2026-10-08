/* Host test for App/app/fmv_bands.c: where the FM Voice build may transmit (frequencies in 10 Hz units). */
#include <stdio.h>
#include "app/fmv_bands.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void)
{
    /* 2 m and 70 cm: lower edge in, upper edge out */
    CHECK(FMV_TxAllowed(14400000) && FMV_TxAllowed(14652000) && FMV_TxAllowed(14799999));
    CHECK(!FMV_TxAllowed(14399999) && !FMV_TxAllowed(14800000));
    CHECK(FMV_TxAllowed(42000000) && FMV_TxAllowed(44625000) && FMV_TxAllowed(44999999));
    CHECK(!FMV_TxAllowed(41999999) && !FMV_TxAllowed(45000000));
    /* FRS / GMRS: 462.550-462.725 and 467.550-467.725, both ends in */
    CHECK(FMV_TxAllowed(46255000) && FMV_TxAllowed(46256250) && FMV_TxAllowed(46272500));
    CHECK(!FMV_TxAllowed(46254999) && !FMV_TxAllowed(46272501));
    CHECK(FMV_TxAllowed(46755000) && FMV_TxAllowed(46771250) && FMV_TxAllowed(46772500));
    CHECK(!FMV_TxAllowed(46754999) && !FMV_TxAllowed(46772501));
    /* MURS: exactly five frequencies */
    CHECK(FMV_TxAllowed(15182000) && FMV_TxAllowed(15188000) && FMV_TxAllowed(15194000));
    CHECK(FMV_TxAllowed(15457000) && FMV_TxAllowed(15460000));
    CHECK(!FMV_TxAllowed(15182001) && !FMV_TxAllowed(15181999) && !FMV_TxAllowed(15458000));
    /* receive-only services and everything else: no */
    CHECK(!FMV_TxAllowed(16255000));     /* NOAA weather 162.550 */
    CHECK(!FMV_TxAllowed(15457500));     /* between the MURS channels */
    CHECK(!FMV_TxAllowed(0) && !FMV_TxAllowed(0xFFFFFFFFu));
    CHECK(!FMV_TxAllowed(44610000 + 20000000)); /* 646.1 MHz */
    CHECK(!FMV_TxAllowed(15000000) && !FMV_TxAllowed(46000000) && !FMV_TxAllowed(46800000));
    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
