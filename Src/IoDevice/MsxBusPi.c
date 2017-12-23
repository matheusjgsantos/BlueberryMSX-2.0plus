//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  Revised: 15-Feb-2013
 
 
// Access from ARM Running Linux
 
#define BCM2708_PERI_BASE        0x3F000000 // Raspberry Pi 2
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
  
#include <stdio.h>
#include <stdlib.h>  
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <bcm2835.h>
#include <time.h>
 
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
 
int  mem_fd;
void *gpio_map;
 
// I/O access
volatile unsigned *gpio;
 
 
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
#define GPIO (*(gpio+13))
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

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
#define SPI_MOSI_PIN 10
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
#define SPI_MOSI	(1<<SPI_MOSI_PIN)
#define SPI_SCLK	(1<<SPI_SCLK_PIN)
#define MSX_SLTSL1  (1<<SLTSL1_PIN)

#define GET_DATA(x) setDataIn(); x = GPIO >> MD00_PIN;
#define SET_DATA(x) setDataOut(); GPIO_CLR = (0xff) << MD00_PIN; GPIO_SET = (x & 0xff) << MD00_PIN;

#define MSX_SET_OUTPUT(g) {INP_GPIO(g); OUT_GPIO(g);}
#define MSX_SET_INPUT(g)  INP_GPIO(g)
#define MSX_SET_CLOCK(g)  INP_GPIO(g); ALT0_GPIO(g)

void setup_io();
int msxread(int slot, unsigned short addr);
void msxwrite(int slot, unsigned short addr, unsigned char byte);
int msxreadio(unsigned short addr);
void msxwriteio(unsigned short addr, unsigned char byte);
void clear_io();
void setDataIn();
void setDataOut();
void spi_clear();
void spi_set(int addr, int rd, int mreq, int slt1);

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

void setDataOut()
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
	*(gpio+1) &= 0x0924927f;
	*(gpio+1) |= 0x09249240;
#endif	
}

void spi_clear()
{
	int x;
	 GPIO_CLR = SPI_CS;
	 GPIO_SET = SPI_MOSI;
	 for(x = 0; x < 24; x++)
	 {
		 GPIO_CLR = SPI_SCLK;
		 GPIO_SET = SPI_SCLK;
	 }
	 GPIO_SET = SPI_CS;
}	

void spi_set(int addr, int rd, int mreq, int slt1)
{
	int cs1, cs12, cs2, wr, iorq, x, spi_data, spi_result;
	cs1 = !(addr & 0xc000 == 0x4000);
	cs2 = !(addr & 0xc000 == 0x8000);
	cs12 = cs1 && cs2;
	iorq = !mreq;
	wr = !rd;
	if (!iorq)
	{
		slt1 = 1;
		GPIO_SET = MSX_SLTSL1;
	}
	else 
	{
		if (slt1 == 2)
			GPIO_CLR = MSX_SLTSL1;
		else
			GPIO_SET = MSX_SLTSL1;
	}
	spi_data = cs12 << 23 | cs1 << 22 | cs2 << 21 | mreq << 20 | iorq << 19 | rd << 18 | wr << 17 | slt1 << 16 | (addr & 0xffff);
	GPIO_CLR = SPI_CS;
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
	GPIO_SET = SPI_CS;
	return;
}
	
#ifdef _MAIN
int main(int argc, char **argv)
{
  int g,rep,i,addr, page=4;
  char byte;
  int offset = 0x4000;
  int size = 0x20000;
  FILE *fp = 0;
  
  if (argc > 1)
  {
	  fp = fopen(argv[1], "wb");
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
 
	msxwrite(1, 0x6000, 3);
	for(addr=offset; addr < offset + size; addr ++)
	{
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
		  if (addr % 16 == 0)
			  printf("\n%04x:", addr);
		  printf ("%02x ", byte);
	  }
	}
	printf("\n");
  clear_io();
  return 0;
 
} // main
#endif
 
 int msxread(int slot, unsigned short addr)
 {
	unsigned char byte;
	struct timespec tv, tr;
	tv.tv_sec = 0;
	tv.tv_nsec = 200;
	spi_set(addr, 0, 0, slot != 1);
//	nanosleep(&tv, &tr);
	GET_DATA(byte);
	//spi_clear();
	return byte;	 
 }
 
 void msxwrite(int slot, unsigned short addr, unsigned char byte)
 {
	struct timespec tv, tr;
	tv.tv_sec = 0;
	tv.tv_nsec = 100;
 	SET_DATA(byte);
	spi_set(addr, 1, 0, slot != 1);
	nanosleep(&tv, &tr);
	spi_clear();
 	SET_DATA(0);
	return;
 }
 
 int msxreadio(unsigned short addr)
 {
	unsigned char byte;
	struct timespec tv, tr;
	tv.tv_sec = 0;
	tv.tv_nsec = 200;
	spi_set(addr, 0, 1, 1);
	//nanosleep(&tv, &tr);
	GET_DATA(byte);
	spi_clear();
	return byte;	 
 }
 
 void msxwriteio(unsigned short addr, unsigned char byte)
 {
	 struct timespec tv, tr;
	 tv.tv_sec = 0;
	 tv.tv_nsec = 400;
 	 SET_DATA(byte);
	 spi_set(addr, 1, 1, 1);
	 nanosleep(&tv, &tr);
	 spi_clear();
 	 SET_DATA(0);
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
 
   close(mem_fd); //No need to keep mem_fd open after mmap
 
   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }
 
   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;

	MSX_SET_OUTPUT(SPI_CS_PIN);
	MSX_SET_OUTPUT(SPI_SCLK_PIN);
	MSX_SET_OUTPUT(SPI_MOSI_PIN);
	MSX_SET_OUTPUT(SLTSL1_PIN);
	OUT_GPIO(MD00_PIN);
	OUT_GPIO(MD01_PIN);
	OUT_GPIO(MD02_PIN);
	OUT_GPIO(MD03_PIN);
	OUT_GPIO(MD04_PIN);
	OUT_GPIO(MD05_PIN);
	OUT_GPIO(MD06_PIN);
	OUT_GPIO(MD07_PIN);	
	GPIO_SET = MSX_SLTSL1 | SPI_CS | SPI_SCLK | SPI_MOSI;  
	SET_DATA(0);
   
} // setup_io

void clear_io()
{
	spi_clear();
}

void msxinit()
{
	setup_io();
}

void msxclose()
{
	clear_io();
}