/*****************************************************************************
**
** Msx Slot Access Code for Raspberry Pi 
** https://github.com/meesokim/msxslot
**
** RPMC(Raspberry Pi MSX Clone) core module
**
** Copyright (C) 2016 Miso Kim meeso.kim@gmail.com
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
*/

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
#define SLTSL3_PIN	RA9
#define SLTSL1_PIN 	RA8
//#define CS12_PIN 	RA9
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
//#define MSX_CS12 (1 << CS12_PIN)
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

#define MSX_CTRL_FLAG (MSX_SLTSL1 | MSX_SLTSL3 | MSX_CS1 | MSX_CS2 | MSX_RD | MSX_WR | MSX_IORQ | MSX_MREQ)

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

#define MSX_CONTROLS	(MSX_SLTSL1 | MSX_SLTSL3 | MSX_MREQ | MSX_IORQ | MSX_RD | MSX_WR | MSX_CS1 | MSX_CS2)

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
void frontled(unsigned char byte);
int msxread(int slot, unsigned short addr);
void msxwrite(int slot, unsigned short addr, unsigned char byte);
int msxreadio(unsigned short addr);
void msxwriteio(unsigned short addr, unsigned char byte);
void clear_io();
void setup_gclk();


void SetAddress(unsigned short addr)
{
	GPIO_CLR = LE_C | 0xffff | DAT_DIR;
	GPIO_SET = LE_A | LE_D | addr;
	GPIO_SET = LE_A;
    GPIO_CLR = LE_A;
	GPIO_SET = LE_C | MSX_CONTROLS;
	GPIO_CLR = LE_D | 0xff;
}	

void SetDelay(int j)
{
	for(int i=0; i<j; i++)
	{
	    while(GPIO & MSX_CLK);
	    while(!(GPIO & MSX_CLK));
	}
}

void SetData(int ioflag, int flag, int delay, unsigned char byte)
{
	GPIO_SET = byte;
	GPIO_CLR = flag | MSX_WR;
	GPIO_SET = ioflag | MSX_WR;
	GPIO_CLR = flag;
//    SetDelay(1);
	GPIO_CLR = MSX_WR;
    SetDelay(delay);
	while(!(GPIO & MSX_WAIT));
	GPIO_SET = MSX_WR;
   	GPIO_SET = MSX_CONTROLS;
	GPIO_CLR = LE_C;

}   

unsigned char GetData(int flag, int rflag, int delay)
{
	unsigned char byte;
	GPIO_SET = DAT_DIR | 0xff;
	GPIO_CLR = flag;
//    SetDelay(1);
	GPIO_CLR = rflag;
	SetDelay(delay);
	while(!(GPIO & MSX_WAIT));
	byte = GPIO;
  	GPIO_SET = LE_D | MSX_CONTROLS;
	GPIO_CLR = LE_C;
	return byte;
}

 int msxread(int slot, unsigned short addr)
 {
	unsigned char byte;
	int cs1, cs2, cs12;
	cs1 = (addr & 0xc000) == 0x4000 ? MSX_CS1: 0;
	cs2 = (addr & 0xc000) == 0x8000 ? MSX_CS2: 0;
	SetAddress(addr);
	byte = GetData((slot == 0 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ, MSX_RD | cs1 | cs2, 1);
#ifdef DEBUG    
	printf("+%04x:%02xr\n", addr, byte);
#endif
	return byte;	 
 }
 
 void msxwrite(int slot, unsigned short addr, unsigned char byte)
 {
	SetAddress(addr);
	SetData(MSX_MREQ, (slot == 0 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ, 2, byte);
#ifdef DEBUG  
	printf("+%04x:%02xw\n", addr, byte);
#endif
	return;
 }
 
 int msxreadio(unsigned short addr)
 {
	unsigned char byte;
	SetAddress(addr);
	byte = GetData(MSX_IORQ, MSX_RD, 2);
#ifdef DEBUG      
	printf("-IO%02x:%02xr\n", addr, byte);
#endif
	return byte;	 
 }
 
 void msxwriteio(unsigned short addr, unsigned char byte)
   {
	SetAddress(addr);
	SetData(MSX_IORQ, MSX_IORQ, 3, byte);
#ifdef DEBUG      
	printf("-IO%02x:%02xw\n", addr, byte);
#endif
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
		bcm2835_gpio_set_pud(i, BCM2835_GPIO_PUD_UP);
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
		freq = 3580000;
		divi = 19200000 / freq ;
		divr = 19200000 % freq ;
		divf = (int)((double)divr * 4096.0 / 19200000.0) ;
		if (divi > 4095)
			divi = 4095 ;		
		divisor = 1 < 12;// | (int)(6648/1024);
		GP_CLK0_CTL = 0x5A000000 | speed_id;    // GPCLK0 off
		while (GP_CLK0_CTL & 0x80);    // Wait for BUSY low
		GP_CLK0_DIV = 0x5A000000 | (divi << 12) | divf; // set DIVI
		usleep(10);
		GP_CLK0_CTL = 0x5A000010 | speed_id;    // GPCLK0 on
		printf("clock enabled: 0x%08x\n", GP_CLK0_CTL );
	}
	else
		printf("clock disabled\n");
	
	bcm2835_gpio_pud(BCM2835_GPIO_PUD_UP);
	for(int i = 0; i < 8; i++)
		bcm2835_gpio_pudclk(i, 1);
	bcm2835_gpio_pudclk(27,1);
	
	GPIO_SET = LE_C | MSX_CONTROLS | MSX_WAIT | MSX_INT;
	GPIO_SET = LE_A | LE_D;
	GPIO_CLR = LE_C | 0xffff;
	GPIO_CLR = LE_C;
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
//	spi_clear();
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
    frontled(0x0);
	printf("MSX BUS initialized\n");
}

void msxclose()
{
	clear_io();
}

int msx_pack_check()
{
	return !(GPIO & SW1);
}
void frontled(unsigned char byte)
{
#define SRCLK (1<<RC22)
#define RCLK (1<<RC23)
#define SER (1<<RC26)
    static unsigned char oldbyte = 0;
    if (oldbyte != byte)
    {
        oldbyte = byte;
        pthread_mutex_lock(&mutex);
        GPIO_CLR = SRCLK | RCLK | SER;
        for (int i = 0; i < 8; i++)
        {
            if ((byte >> i) & 1)
                GPIO_SET = SER;
            else
                GPIO_CLR = SER;
            GPIO_SET = SRCLK;
            GPIO_CLR = SRCLK;
        }
        GPIO_SET = RCLK;
        pthread_mutex_unlock(&mutex);	    
    }
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
  int slot = 0;
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
			msxwrite(slot, 0x6000, page++);
			printf("page:%d, address=0x%04x\n", page-1, addr );
		 }
		 byte = msxread(1, 0x6000 + (addr & 0x1ffff));
	  }
	  else
	  {
		  byte = msxread(slot, addr);
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
			byte0 = msxread(slot, addr);
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
