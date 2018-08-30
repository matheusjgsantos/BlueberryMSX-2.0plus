//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  Revised: 15-Feb-2013

#define RPMC_V5
#include <bcm2835.h>
// Access from ARM Running Linux
#include "rpi-gpio.h"
   
#include <stdio.h>
#include <stdlib.h>  
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <bcm2835.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>

#include "barrier.h"
 
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
 
int  mem_fd;
void *gpio_map;
 
// I/O access
volatile unsigned *gpio;
volatile unsigned *gpio10;
volatile unsigned *gpio7;
volatile unsigned *gpio13;
volatile unsigned *gpio1;
volatile unsigned *gclk_base;
 
 
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio10) // clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g) (*(gpio13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
#define GPIO (*(gpio13))
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

#define GZ_CLK_BUSY    (1 << 7)
#define GP_CLK0_CTL *(gclk_base + 0x1C)
#define GP_CLK0_DIV *(gclk_base + 0x1D)

#ifdef RPMC_V5
#define RD0		0
#define RD1		1
#define RD2		2
#define RD3		3
#define RD4		4
#define RD5		5
#define RD6		6
#define RD7		7
#define RA8		8
#define RA9		9
#define RA10	10
#define RA11	11
#define RA12	12
#define RA13	13
#define RA14	14
#define RA15	15
#define RC16	16
#define RC17	17
#define RC18	18
#define RC19	19
#define RC20	20
#define RC21	21
#define RC22	22
#define RC23	23
#define RC24	24
#define RC25	25
#define RC26	26
#define RC27	27

#define MD00_PIN 	0
#define SLTSL3_PIN	RC26
#define SLTSL1_PIN 	RA8
#define CS12_PIN 	RA9
#define CS1_PIN		RA10
#define CS2_PIN 	RA11
#define RD_PIN		RA12
#define WR_PIN		RA13
#define IORQ_PIN	RA14
#define MREQ_PIN	RA15
#define LE_A_PIN	RC16
#define LE_C_PIN	RC17
#define LE_D_PIN	RC18
#define RESET_PIN	RC19
#define CLK_PIN		RC20
#define INT_PIN		RC24
#define WAIT_PIN	RC25
#define BUSDIR_PIN	RC26
#define SW1_PIN		RC27

#define MSX_SLTSL1 (1 << SLTSL1_PIN)
#define MSX_SLTSL3 (1 << SLTSL3_PIN)
#define MSX_CS1	(1 << CS1_PIN)
#define MSX_CS2 (1 << CS2_PIN)
#define MSX_CS12 (1 << CS12_PIN)
#define MSX_RD	(1 << RD_PIN)
#define MSX_WR  (1 << WR_PIN)
#define MSX_IORQ  (1 << IORQ_PIN)
#define MSX_MREQ	(1 << MREQ_PIN)
#define MSX_RESET (1 << RESET_PIN)
#define MSX_WAIT	(1 << WAIT_PIN)
#define MSX_INT		(1 << INT_PIN)
#define LE_A	(1 << LE_A_PIN)
#define LE_C	(1 << LE_C_PIN)
#define LE_D	(1 << LE_D_PIN)
#define MSX_CLK (1 << CLK_PIN)
#define SW1 	(1 << SW1_PIN)
#define DAT_DIR (1 << RC21)

#define MSX_CTRL_FLAG (MSX_SLTSL1 | MSX_SLTSL3 | MSX_CS1 | MSX_CS2 | MSX_CS12 | MSX_RD | MSX_WR | MSX_IORQ | MSX_MREQ | DAT_DIR)

#else
// MSX slot access macro
#define MD00_PIN	12
#define MD01_PIN	13
#define MD02_PIN	14
#define MD03_PIN	15
#define MD04_PIN	16
#define MD05_PIN	17
#define MD06_PIN	18
#define MD07_PIN	19
#define SLTSL1_PIN	3
#define MCLK_PIN	4
#define SPI_CS_PIN	8
#ifdef RPMC_V4
#define SPI_MOSI0_PIN 7
#define SPI_MOSI1_PIN 9
#define SPI_MOSI2_PIN 10
#define SPI_MOSI0	(1<<SPI_MOSI0_PIN)
#define SPI_MOSI1	(1<<SPI_MOSI1_PIN)
#define SPI_MOSI2	(1<<SPI_MOSI2_PIN)
#else
#define SPI_MOSI_PIN 10
#define SPI_MOSI	(1<<SPI_MOSI_PIN)
#endif
#define SPI_SCLK_PIN 11

#define MSX_SLTSL0	(1<<0)
#define MSX_WR		(1<<1)
#define MSX_RD		(1<<2)
#define MSX_IORQ	(1<<3)
#define MSX_MERQ	(1<<4)
#define MSX_CS2		(1<<5)
#define MSX_CS1		(1<<6)
#define MSX_CS12	(1<<7)

#define SPI_CS		(1<<SPI_CS_PIN)
#define SPI_SCLK	(1<<SPI_SCLK_PIN)
#define MSX_SLTSL1  (1<<SLTSL1_PIN)

#endif

#ifdef RPMC_V5
#define GET_DATA(x) x = GPIO & 0xff; //(GPIO >> MD00_PIN) & 0xff;
#define SET_DATA(x) GPIO_SET = x << MD00_PIN;
#else
#define GET_DATA(x) GPIO_CLR = 0xff << MD00_PIN; x = GPIO >> MD00_PIN; 
#define SET_DATA(x) GPIO_CLR = 0xff << MD00_PIN; GPIO_SET = (x & 0xff) << MD00_PIN;
#endif
#define MSX_SET_OUTPUT(g) {INP_GPIO(g); OUT_GPIO(g);}
#define MSX_SET_INPUT(g)  INP_GPIO(g)
#define MSX_SET_CLOCK(g)  INP_GPIO(g); ALT0_GPIO(g)

pthread_mutex_t mutex;

int setup_io();
int msxread(int slot, unsigned short addr);
void msxwrite(int slot, unsigned short addr, unsigned char byte);
int msxreadio(unsigned short addr);
void msxwriteio(unsigned short addr, unsigned char byte);
void clear_io();
void setDataIn();
void setDataOut();
void spi_clear();
void spi_set(int addr, int rd, int mreq, int slt1);
void setup_gclk();

void setDataIn()
{
#if 0
	INP_GPIO(MD00_PIN);
	INP_GPIO(MD01_PIN);
	INP_GPIO(MD02_PIN);
	INP_GPIO(MD03_PIN);
	INP_GPIO(MD04_PIN);
	INP_GPIO(MD05_PIN);
	INP_GPIO(MD06_PIN);
	INP_GPIO(MD07_PIN);
#else
	*(gpio+1) &= 0x03f;
#endif	
}

/*
0099 9888 7776 6655 5444 3332 2211 1000

0099 9888 7776 6655 5444 3332 2211 1000
  00 1001 0010 0100 1001 0010 01xx xxxx
   0    9    2    4    9    2    7    f
0099 9888 7776 6655 5444 3332 2211 1000
*/

void inline setDataOut()
{
#if 0
	OUT_GPIO(MD00_PIN);
	OUT_GPIO(MD01_PIN);
	OUT_GPIO(MD02_PIN);
	OUT_GPIO(MD03_PIN);
	OUT_GPIO(MD04_PIN);
	OUT_GPIO(MD05_PIN);
	OUT_GPIO(MD06_PIN);
	OUT_GPIO(MD07_PIN);
#else
	*(gpio1) &= 0x0924927f;
	*(gpio1) |= 0x09249240;
	GPIO_SET = 0xff << MD00_PIN;	
#endif	
}

void inline spi_clear()
{
	int x;
#ifdef RPMC_V5
	GPIO_SET = LE_A | 0xffff;
	GPIO_CLR = LE_C | LE_D; 
#else
#ifdef RPMC_V4
	 GPIO_SET = SPI_SCLK | SPI_MOSI0 | SPI_MOSI1 | SPI_MOSI2;
	 GPIO_CLR = SPI_CS | SPI_SCLK;
#else	 
	 GPIO_CLR = SPI_SCLK;
	 GPIO_SET = SPI_MOSI;
	 for(x = 0; x < 24; x++)
	 {
		 GPIO_CLR = SPI_SCLK;
		 GPIO_SET = SPI_SCLK;
	 }
#endif	 
	 GPIO_SET = SPI_CS;
#endif	 
}	

void inline spi_set(int addr, int rd, int mreq, int slt1)
{
	int cs1, cs12, cs2, wr, iorq, x, spi_data, spi_result;
	int set, clr;
	if (!mreq) 
	{
		cs1 = !((addr & 0xc000) == 0x4000);
		cs2 = !((addr & 0xc000) == 0x8000);
		cs12 = cs1 && cs2;
	} 
	else
	{
		cs1 = cs2 = cs12 = 1;
	}
#ifdef RPMC_V5
#else
#if 1
	if (slt1 == 2)
		GPIO_CLR = MSX_SLTSL1;
	else
		GPIO_SET = MSX_SLTSL1;
#endif	
	GPIO_CLR = SPI_CS;
#if 0	
	spi_data = cs12 << 23 | cs1 << 22 | cs2 << 21 | mreq << 20 | !mreq << 19 | rd << 18 | !rd << 17 | slt1 << 16 | (addr & 0xffff);
	for(x = 23; x >= 0; x--)
	{
		GPIO_CLR = SPI_SCLK;
		if (spi_data & (1<<x))
		{
			GPIO_SET = SPI_MOSI;
		}
		else
		{
			GPIO_CLR = SPI_MOSI;
		}
		GPIO_SET = SPI_SCLK;
	}
#else
#ifdef RPMC_V4	
#if 1
	clr = SPI_SCLK | SPI_MOSI0 | SPI_MOSI1 | SPI_MOSI2;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (cs12)			set |= SPI_MOSI0;
	if (addr & 1<<15)	set |= SPI_MOSI1;
	if (addr & 1<<7)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (cs1)			set |= SPI_MOSI0; 
	if (addr & 1<<14)	set |= SPI_MOSI1;
	if (addr & 1<<6)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (cs2)			set |= SPI_MOSI0;
	if (addr & 1<<13)	set |= SPI_MOSI1;
	if (addr & 1<<5)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (mreq)			set |= SPI_MOSI0;
	if (addr & 1<<12)	set |= SPI_MOSI1;
	if (addr & 1<<4)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (!mreq)			set |= SPI_MOSI0;
	if (addr & 1<<11)	set |= SPI_MOSI1;
	if (addr & 1<<3)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (rd)				set |= SPI_MOSI0;
	if (addr & 1<<10)	set |= SPI_MOSI1;
	if (addr & 1<<2)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (!rd)			set |= SPI_MOSI0; 
	if (addr & 1<<9)	set |= SPI_MOSI1;
	if (addr & 1<<1)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
	set = SPI_SCLK;
	if (slt1 != 1 && !mreq)	set |= SPI_MOSI0;
	if (addr & 1<<8)	set |= SPI_MOSI1;
	if (addr & 1<<0)	set |= SPI_MOSI2;
	GPIO_SET = set;
	GPIO_CLR = clr;
#else
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	if (cs12)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<15)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<7)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (cs1)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (addr & 1<<14)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<6)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (cs2)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<13)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (addr & 1<<5)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (mreq)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<12)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<4)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (!mreq)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<11)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<3)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (rd)				set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (addr & 1<<10)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<2)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (!rd)			set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<9)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	GPIO_CLR = clr;
	GPIO_SET = set;
	GPIO_CLR = SPI_SCLK; 
	set = SPI_SCLK;
	clr = 0;
	if (addr & 1<<1)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	if (slt1 != 1)		set |= SPI_MOSI0; else clr |= SPI_MOSI0; 
	if (addr & 1<<8)	set |= SPI_MOSI1; else clr |= SPI_MOSI1; 
	if (addr & 1<<0)	set |= SPI_MOSI2; else clr |= SPI_MOSI2; 
	GPIO_CLR = clr;
	GPIO_SET = set;
#endif	
#else
	GPIO_CLR = SPI_SCLK; if (cs12)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (cs1)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (cs2)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (mreq)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (!mreq)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (rd)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (!rd)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (slt1 != 1)	GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;	
	GPIO_CLR = SPI_SCLK; if (addr & 1<<15) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<14) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<13) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<12) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<11) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<10) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<9) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<8) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<7) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<6) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<5) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<4) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<3) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<2) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1<<1) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
	GPIO_CLR = SPI_SCLK; if (addr & 1) GPIO_SET = SPI_MOSI; else GPIO_CLR = SPI_MOSI; GPIO_SET = SPI_SCLK;
#endif	
#endif	
	GPIO_SET = SPI_CS;
#endif
	return;
}

void SetAddress(unsigned short addr)
{
	GPIO_CLR = 0xffff;
	GPIO_SET = LE_A | addr;
	GPIO_SET = LE_A;
    GPIO_CLR = LE_A;
	GPIO_SET = LE_C | MSX_CTRL_FLAG;
    GPIO_SET = LE_C;
    GPIO_CLR = LE_C | LE_D;
}	

void SetDelay(int j)
{
	for(int i=0; i<j; i++)
		GPIO_SET = 0;
}

void SetData(int flag, int delay, unsigned char byte)
{
	GPIO_CLR = flag | LE_D | DAT_DIR | 0xff;
	GPIO_SET = MSX_WR;
	GPIO_SET = LE_C | byte;
	for(int i=0; i < 5; i++)
		GPIO_SET = 0;
	GPIO_CLR = MSX_WR;
	for(int i=0; i < delay; i++)
	{
		GPIO_SET = LE_C | byte;
	}
	GPIO_SET = MSX_WR;
	byte = GPIO;
	byte = GPIO;
	GPIO_SET = LE_D | MSX_CTRL_FLAG | DAT_DIR;   	
	GPIO_CLR = LE_C;
}   

unsigned char GetData(int flag, int delay)
{
	unsigned char byte;
	GPIO_SET = LE_C | DAT_DIR;
	GPIO_CLR = MSX_CLK | flag;
	SetDelay(delay);
	byte = GPIO;
	GPIO_SET = LE_D;
	GPIO_SET = MSX_CTRL_FLAG | MSX_CLK;
   	GPIO_SET = MSX_CLK;
   	GPIO_CLR = LE_C | MSX_CLK;
   	GPIO_SET = MSX_CLK;
	return byte;
}

 int msxread(int slot, unsigned short addr)
 {
	unsigned char byte;
	int cs1, cs2, cs12;
	cs1 = (addr & 0xc000) == 0x4000 ? MSX_CS1: 0;
	cs2 = (addr & 0xc000) == 0x8000 ? MSX_CS2: 0;
	cs12 = (cs1 | cs2 ? MSX_CS12 : 0);
	if (GPIO & SW1)
		return 0xff;
	pthread_mutex_lock(&mutex);
	SetAddress(addr);
	byte = GetData((slot == 1 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ | MSX_RD | cs1 | cs2 | cs12, 25);
	pthread_mutex_unlock(&mutex);	
	return byte;	 
 }
 
 void msxwrite(int slot, unsigned short addr, unsigned char byte)
 {
	pthread_mutex_lock(&mutex);
	SetAddress(addr);
	SetData((slot == 1 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ, 35, byte);
	pthread_mutex_unlock(&mutex);	
	return;
 }
 
 int msxreadio(unsigned short addr)
 {
	unsigned char byte;
	pthread_mutex_lock(&mutex);
	SetAddress(addr);
	byte = GetData(MSX_IORQ | MSX_RD, 35);
	pthread_mutex_unlock(&mutex);	
	return byte;	 
 }
 
 void msxwriteio(unsigned short addr, unsigned char byte)
   {
	pthread_mutex_lock(&mutex);
	SetAddress(addr);
	SetData(MSX_IORQ, 45, byte);
	pthread_mutex_unlock(&mutex);		
	return;
 }
 
#if 0 
int rtapi_open_as_root(const char *filename, int mode) {
	fprintf (stderr, "euid: %d uid %d\n", geteuid(), getuid());
	seteuid(0);
	fprintf (stderr, "euid: %d uid %d\n", geteuid(), getuid());
	setfsuid(geteuid());
	int r = open(filename, mode);
	setfsuid(getuid());
	return r;
}
#endif
//
// Set up a memory regions to access GPIO
//
int setup_io()
{
	int i, speed_id, divisor ;	
	if (!bcm2835_init()) return -1;
	gpio = bcm2835_regbase(BCM2835_REGBASE_GPIO);
	for(int i=0; i < 27; i++)
	{
		bcm2835_gpio_fsel(i, 1);    
	}

	gpio10 = gpio+10;
	gpio7 = gpio+7;
	gpio13 = gpio+13;
	gpio1 = gpio+1;
	//SET_GPIO_ALT(20, 5);
	gclk_base = bcm2835_regbase(BCM2835_REGBASE_CLK);
	if (gclk_base != MAP_FAILED)
	{
		int divi, divr, divf, freq;
		bcm2835_gpio_fsel(20, BCM2835_GPIO_FSEL_ALT5); // GPIO_20
		speed_id = 1;
		freq = 3500000;
		divi = 19200000 / freq ;
		divr = 19200000 % freq ;
		divf = (int)((double)divr * 4096.0 / 19200000.0) ;
		if (divi > 4095)
			divi = 4095 ;		
		divisor = 1 < 12;// | (int)(6648/1024);
		GP_CLK0_CTL = 0x5A000000 | speed_id;    // GPCLK0 off
		while (GP_CLK0_CTL & 0x80);    // Wait for BUSY low
		GP_CLK0_DIV = 0x5A000000 | (divi << 12) | divf; // set DIVI
		GP_CLK0_CTL = 0x5A000010 | speed_id;    // GPCLK0 on
		printf("clock enabled: 0x%08x\n", GP_CLK0_CTL );
	}
	else
		printf("clock disabled\n");
	
	bcm2835_gpio_pud(BCM2835_GPIO_PUD_UP);
	for(int i = 0; i < 8; i++)
		bcm2835_gpio_pudclk(i, 1);
	bcm2835_gpio_pudclk(27,1);
	
//	GPIO_SET = LE_C | MSX_IORQ | MSX_RD | MSX_WR | MSX_MREQ | MSX_CS1 | MSX_CS2 | MSX_CS12 | MSX_SLTSL1 | MSX_SLTSL3;
//	GPIO_SET = LE_A | LE_D;
	GPIO_CLR = 0xffff;
	GPIO_CLR = MSX_RESET;
	for(int i=0;i<2000000;i++);
	GPIO_SET = MSX_RESET;
	for(int i=0;i<1000000;i++);
	return 0;
} // setup_io

#if 0
#define EINVAL 0
int stick_this_thread_to_core(int core_id) {
   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
   if (core_id < 0 || core_id >= num_cores)
      return EINVAL;

   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(core_id, &cpuset);
   return sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}
#endif
void clear_io()
{
	spi_clear();
}

void msxinit()
{
	const struct sched_param priority = {1};
	sched_setscheduler(0, SCHED_FIFO, &priority);  
//	stick_this_thread_to_core(0);
	if (setup_io() == -1)
    {
        printf("GPIO init error\n");
        exit(0);
    }
	printf("MSX BUS initialized\n");
}

void msxclose()
{
	clear_io();
}


#ifdef _MAIN


int main(int argc, char **argv)
{
  int g,rep,i,addr, page=4, c= 0,addr0;
  char byte, byte0, io;
  int offset = 0x4000;
  int size = 0x8000;
  FILE *fp = 0;
  struct timespec t1, t2;
  double elapsedTime = 0;
  int binary = 0;
  io = 0;
  if (argc > 1)
  {
	 if (strcmp(argv[1], "io"))
		fp = fopen(argv[1], "wb");
	 else
		io = 1;
  }
  if (argc > 2)
  {
	  offset = atoi(argv[2]);
  }
  if (argc > 3)
  {
	  size = atoi(argv[3]);
  }
 
  // Set up gpi pointer for direct register access
  setup_io();
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
	if (io > 0)
	{
		for(i = 0; i < 256; i++)
		{
			if (i % 16 == 0)
				printf("%02x: ", i);
			printf("%02x ", msxreadio(i));
			if (i > 0 && i % 16 == 15)
				printf("\n");
		}
		exit(0);
	}
	msxwrite(1, 0x6000, 3);
	offset = 0x4000;
	for(addr=offset; addr < offset + size; addr ++)
	{
#if 0
		addr0 = 0xffff & (addr + (rand() % 2));//0xffff & (0x4000 + rand());
		printf("%04x:%02x\n", addr0, 0xff & msxread(1, addr0));
		addr0 = 0xffff & (addr + (rand() % 2));//0xffff & (0x4000 + rand());
		printf("%04x:%02x\n", addr0, 0xff & msxread(1, addr0));
#else		
	  if (addr > 0xbfff)
	  {
		 if (!(addr & 0x1fff)) {
			msxwrite(1, 0x6000, page++);
			printf("page:%d, address=0x%04x\n", page-1, addr );
		 }
		 byte = msxread(1, 0x6000 + (addr & 0x1ffff));
	  }
	  else
	  {
		  byte = msxread(1, addr);
	  }
	  if (fp)
		  fwrite(&byte, 1, 1, fp);
	  else
	  {
#if 1		 
		if (addr % 16 == 0)
			 printf("\n%04x:", addr);
#if 1		  
		c = 0;
		for(i=0;i<10;i++)
		{
			byte0 = msxread(1, addr);
			if (byte != byte0)
				c = 1;
		}
		if (c)  
			printf("\e[31m%02x \e[0m", byte);
		else
			printf("%02x ", byte);
#else
		printf("%02x ", byte);
#endif	
#endif
	  }
#endif
	}
	printf("\n");
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t2);
	elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000000000.0;      // sec to ns
	elapsedTime += (t2.tv_nsec - t1.tv_nsec) ;   // us to ns	
	if (!binary) {
		printf("elapsed time: %10.2fs, %10.2fns/i\n", elapsedTime/100000000, elapsedTime / size);
	}	
  clear_io();
  return 0;
 
} // main
#endif
