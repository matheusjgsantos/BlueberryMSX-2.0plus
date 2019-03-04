#include <bcm2835.h>
#include <stdlib.h>
#include <sys/mman.h>

void main(int argc, char *argv[])
{
    int mA = 12;
    if (argc > 1)
        mA = atoi(argv[0]);
    if (mA >= 2 && mA <= 16)
        mA = (mA - 2) / 2;
    else
        mA = 0;
    bcm2835_init();
	if (bcm2835_regbase(BCM2835_REGBASE_PADS) != MAP_FAILED)
		bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_0_27, BCM2835_PAD_HYSTERESIS_ENABLED | mA);
	else
		printf("PADS access denied.\n");
}
