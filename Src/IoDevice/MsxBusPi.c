//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  Revised: 15-Feb-2013
 
 
// Access from ARM Running Linux
 
#define BCM2708_PERI_BASE        0x3F000000 // Raspberry Pi 2
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define CLOCK_BASE				 (BCM2708_PERI_BASE + 0x101000) /* CLK controller */
  
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <bcm2835.h>
#include <time.h>
 
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
#define PI_ALT0   4

int  mem_fd;
void *gpio_map;
 
// I/O access
volatile unsigned *gpio;
volatile unsigned *clkReg;

#define CLK_PASSWD  (0x5A<<24)

#define CLK_CTL_MASH(x)((x)<<9)
#define CLK_CTL_BUSY    (1 <<7)
#define CLK_CTL_KILL    (1 <<5)
#define CLK_CTL_ENAB    (1 <<4)
#define CLK_CTL_SRC(x) ((x)<<0)

#define CLK_SRCS 4

#define CLK_CTL_SRC_OSC  1  /* 19.2 MHz */
#define CLK_CTL_SRC_PLLC 5  /* 1000 MHz */
#define CLK_CTL_SRC_PLLD 6  /*  500 MHz */
#define CLK_CTL_SRC_HDMI 7  /*  216 MHz */

#define CLK_DIV_DIVI(x) ((x)<<12)
#define CLK_DIV_DIVF(x) ((x)<< 0)

#define CLK_GP0_CTL 28
#define CLK_GP0_DIV 29
#define CLK_GP1_CTL 30
#define CLK_GP1_DIV 31
#define CLK_GP2_CTL 32
#define CLK_GP2_DIV 33

#define CLK_PCM_CTL 38
#define CLK_PCM_DIV 39

#define CLK_PWM_CTL 40
#define CLK_PWM_DIV 41 
 
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define ALT0_GPIO(g) *(gpio+((g)/10)) |=  (4<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
#define GET_GPIOS(s,g) (*(gpio+13)&(s<<g)) // 0 if LOW, (1<<g) if HIGH
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock




// MSX slot access macro
#define MCS_PIN     0 
#define MA0_PIN     1
#define MODE_PIN    2
#define MREADY_PIN  3
#define MCLK_PIN    4
#define MD00_PIN    5
#define MD01_PIN    6
#define MD02_PIN    7
#define MD03_PIN    8
#define MD04_PIN    9
#define MD05_PIN    10
#define MD06_PIN    11
#define MD07_PIN    12
#define MD08_PIN    13
#define MD09_PIN    14
#define MD10_PIN    15
#define MD11_PIN    16
#define MD12_PIN    17
#define MD13_PIN    18
#define MD14_PIN    19
#define MD15_PIN    20
#define RW_PIN      21
#define MIO_PIN     22
#define SLTSL_PIN   23
#define RESET_PIN   24
#define M1_PIN      25

#define MSX_CS       1
#define MSX_MA0     (1<<MA0_PIN)
#define MSX_MODE    (1<<MODE_PIN)
#define MSX_READY   (1<<MREADY_PIN)
#define MSX_RW      (1<<RW_PIN)
#define MSX_MIO     (1<<MIO_PIN)
#define MSX_SLTSL   (1<<SLTSL_PIN)
#define MSX_RESET   (1<<RESET_PIN)
#define MSX_M1      (1<<M1_PIN)

#define SET_ADDR(x) setDataOut(); GPIO_CLR = 0xffff << MD00_PIN; GPIO_SET = (x & 0xffff) << MD00_PIN
#define GET_DATA(x) x = GET_GPIOS(0xff,MD00_PIN) >> MD00_PIN; 
#define SET_DATA(x) GPIO_CLR = 0xff << MD00_PIN; GPIO_SET = (x & 0xff) << MD00_PIN

#define MSX_SET_OUTPUT(g) {INP_GPIO(g); OUT_GPIO(g);}
#define MSX_SET_INPUT(g)  INP_GPIO(g)
#define MSX_SET_CLOCK(g)  INP_GPIO(g); ALT0_GPIO(g)

#include "MsxBusPi.h"
void setup_io();
void clear_io();

static int initClock(int clock, int source, int divI, int divF, int MASH)
{
   int ctl[] = {CLK_GP0_CTL, CLK_GP2_CTL};
   int div[] = {CLK_GP0_DIV, CLK_GP2_DIV};
   int src[CLK_SRCS] =
      {CLK_CTL_SRC_PLLD,
       CLK_CTL_SRC_OSC,
       CLK_CTL_SRC_HDMI,
       CLK_CTL_SRC_PLLC};

   int clkCtl, clkDiv, clkSrc;
   uint32_t setting;

   if ((clock  < 0) || (clock  > 1))    return -1;
   if ((source < 0) || (source > 3 ))   return -2;
   if ((divI   < 2) || (divI   > 4095)) return -3;
   if ((divF   < 0) || (divF   > 4095)) return -4;
   if ((MASH   < 0) || (MASH   > 3))    return -5;

   clkCtl = ctl[clock];
   clkDiv = div[clock];
   clkSrc = src[source];

   clkReg[clkCtl] = CLK_PASSWD | CLK_CTL_KILL;

   /* wait for clock to stop */

   while (clkReg[clkCtl] & CLK_CTL_BUSY)
   {
      usleep(10);
   }

   clkReg[clkDiv] =
      (CLK_PASSWD | CLK_DIV_DIVI(divI) | CLK_DIV_DIVF(divF));

   usleep(10);

   clkReg[clkCtl] =
      (CLK_PASSWD | CLK_CTL_MASH(MASH) | CLK_CTL_SRC(clkSrc));

   usleep(10);

   clkReg[clkCtl] |= (CLK_PASSWD | CLK_CTL_ENAB);
}

static int termClock(int clock)
{
   int ctl[] = {CLK_GP0_CTL, CLK_GP2_CTL};

   int clkCtl;

   if ((clock  < 0) || (clock  > 1))    return -1;

   clkCtl = ctl[clock];

   clkReg[clkCtl] = CLK_PASSWD | CLK_CTL_KILL;

   /* wait for clock to stop */

   while (clkReg[clkCtl] & CLK_CTL_BUSY)
   {
      usleep(10);
   }
}
/*
#0001 1122 2333 4445 5566 6777 8889 9900
                   1 0010 0100 1001 0000
				   8  4    2   9
#aaab bbcc cddd eeef ff
 1001 0010 0100 1001 00				   
*/ 
void setDataIn()
{
#if 1
	*gpio =  0x00004049;
	*(gpio+1) = 0x09249200;
#else	
	INP_GPIO(MD00_PIN);
	INP_GPIO(MD01_PIN);
	INP_GPIO(MD02_PIN);
	INP_GPIO(MD03_PIN);
	INP_GPIO(MD04_PIN);
	INP_GPIO(MD05_PIN);
	INP_GPIO(MD06_PIN);
	INP_GPIO(MD07_PIN);
#endif	
}

void setDataOut()
{
#if 1
	*gpio = 0x0924c049;
	*(gpio+1) = 0x09249249;
#else	
	OUT_GPIO(MD00_PIN);
	OUT_GPIO(MD01_PIN);
	OUT_GPIO(MD02_PIN);
	OUT_GPIO(MD03_PIN);
	OUT_GPIO(MD04_PIN);
	OUT_GPIO(MD05_PIN);
	OUT_GPIO(MD06_PIN);
	OUT_GPIO(MD07_PIN);
#endif	
}

int msxinit()
{
	int i;
	setup_io();

	MSX_SET_OUTPUT(MCS_PIN);
	MSX_SET_OUTPUT(MA0_PIN);  
	MSX_SET_OUTPUT(MODE_PIN);  
	MSX_SET_OUTPUT(RW_PIN);
	MSX_SET_OUTPUT(MIO_PIN);
	MSX_SET_OUTPUT(SLTSL_PIN);
	initClock(0, 0, 10, 492, 0);
	MSX_SET_CLOCK(MCLK_PIN);

	usleep(1000);
	for (i = 0; i < 16; i++)
	{
		INP_GPIO(MD00_PIN+i);
		OUT_GPIO(MD00_PIN+i);
	}
}

int msxclose()
{
	clear_io();	
}
 
 int msxread(int slot, unsigned short addr)
 {
	 unsigned char byte;
	 GPIO_CLR = MSX_MA0 | MSX_MODE;
	 GPIO_SET = MSX_CS | MSX_RESET | MSX_M1 | (slot !=1 ? MSX_SLTSL : 0);
	 SET_ADDR(addr);
	 GPIO_CLR = MSX_RW | MSX_MIO | (slot == 1 ? MSX_SLTSL : 0);
	 GPIO_CLR = MSX_CS;
	 while(!GET_GPIO(MREADY_PIN));
	 GPIO_SET = MSX_MA0;
	 setDataIn();
	 GPIO_CLR = MSX_CS;
	 //while(!GET_GPIO(MREADY_PIN));
	 GET_DATA(byte);	 
//	 setDataOut();
	 GPIO_SET = MSX_CS | MSX_MODE;
	 return byte;	 
 }
 
 void msxwrite(int slot, unsigned short  addr, unsigned char byte)
 {
	 int i = 0;
	 GPIO_CLR = MSX_MA0 | MSX_MODE;
	 GPIO_SET = MSX_CS | MSX_RESET | MSX_M1 | (slot != 1 ? MSX_SLTSL : 0);
	 SET_ADDR(addr);
	 GPIO_SET = MSX_RW;
	 GPIO_CLR = MSX_MIO | (slot == 1 ? MSX_SLTSL : 0);
	 GPIO_CLR = MSX_CS;
	 while(!GET_GPIO(MREADY_PIN));
	 SET_DATA(byte);	 
	 GPIO_SET = MSX_MA0;
	 GPIO_CLR = MSX_CS;
	 while(!GET_GPIO(MREADY_PIN));
	 GPIO_SET = MSX_CS | MSX_MODE;
	 return;
 }

 int msxreadio(unsigned short  addr)
 {
	 unsigned char byte;
	 GPIO_SET = MSX_CS | MSX_RESET | MSX_M1;
	 GPIO_CLR = MSX_MA0 | MSX_MODE;
	 SET_ADDR(addr);
	 GPIO_SET = MSX_MIO;
	 GPIO_CLR = MSX_RW;
	 GPIO_CLR = MSX_CS;
	 while(!GET_GPIO(MREADY_PIN));
	 GPIO_SET = MSX_CS;
	 GPIO_SET = MSX_MA0;
	 setDataIn();	 
	 GPIO_CLR = MSX_CS;
	 while(!GET_GPIO(MREADY_PIN));
	 GET_DATA(byte);	 
	 GPIO_SET = MSX_CS | MSX_MODE;
	 setDataOut();	 
	 return byte;	 
 }
 
 void msxwriteio(unsigned short  addr, unsigned char byte)
 {
	 GPIO_SET = MSX_CS;
	 GPIO_CLR = MSX_MA0 | MSX_MODE;
	 SET_ADDR(addr);
	 GPIO_CLR = MSX_CS;
	 while(!GET_GPIO(MREADY_PIN));
	 GPIO_SET = MSX_CS;
	 SET_DATA(byte);	 
	 GPIO_SET = MSX_MA0;
	 GPIO_CLR = MSX_RW;
	 GPIO_CLR = MSX_CS;
	 while(!GET_GPIO(MREADY_PIN));
	 GPIO_SET = MSX_CS | MSX_MODE;
	 return;
 }
 
 //
// Set up a memory regions to access GPIO
//
void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }
 
   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );
   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }
   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;

#define CLK_LEN   0xA8   
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      CLK_LEN,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      CLOCK_BASE         //Offset to GPIO peripheral
   );
    if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }
   // Always use volatile pointer!
   clkReg = (volatile unsigned *)gpio_map;
   
   close(mem_fd); //No need to keep mem_fd open after mmap

   if (!bcm2835_init())
   {
		printf("bcm2835_init error\n");
		exit(-1);   
   }
#if 0   
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);      // The default
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                   // The default
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_4); 
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                      // The default
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);      // the default   
#endif
} // setup_io

void clear_io()
{
#if 0
    bcm2835_spi_end();
#endif
    bcm2835_close();
}