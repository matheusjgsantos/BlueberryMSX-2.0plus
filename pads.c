#include <bcm2835.h>
#include <stdlib.h>
#include <sys/mman.h>

// plaire
#include <stdio.h>
// plaire 
#define GP_CLK0_CTL *(gclk_base + 0x1C)
#define GP_CLK0_DIV *(gclk_base + 0x1D)

void main(int argc, char *argv[])
{
	volatile unsigned *gclk_base;
    int mA = 12;
    if (argc > 1)
        mA = atoi(argv[0]);
    if (mA >= 2 && mA <= 16)
        mA = (mA - 2) / 2;
    else
        mA = 0;
    bcm2835_init();
	gclk_base = bcm2835_regbase(BCM2835_REGBASE_CLK);
	if (gclk_base != MAP_FAILED)
	{
		int divi, divr, divf, freq;
		bcm2835_gpio_fsel(20, BCM2835_GPIO_FSEL_ALT5); // GPIO_20
		int speed_id = 1;
		freq = 3579545;  // 3.579545Mhz
		divi = 19200000 / freq ;
		divr = 19200000 % freq ;
		divf = (int)((double)divr * 4096.0 / 19200000.0) ;
		if (divi > 4095)
			divi = 4095 ;		
		int divisor = 1 < 12;// | (int)(6648/1024);
		GP_CLK0_CTL = 0x5A000000 | speed_id;    // GPCLK0 off
		while (GP_CLK0_CTL & 0x80);    // Wait for BUSY low
		GP_CLK0_DIV = 0x5A000000 | (divi << 12) | divf; // set DIVI
		GP_CLK0_CTL = 0x5A000010 | speed_id;    // GPCLK0 on
		printf("clock enabled: 0x%08x\n", GP_CLK0_CTL );
	}
	else
		printf("clock disabled\n");
	
	if (bcm2835_regbase(BCM2835_REGBASE_PADS) != MAP_FAILED)
	{
		bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_0_27, BCM2835_PAD_HYSTERESIS_ENABLED | mA);
		printf("Pad strength controlled\n");
	}
	else
		printf("PADS access denied.\n");
}
