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
// Access from ARM Running Linux
#include "rpi-gpio.h"
    
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>  
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include <pthread.h>

#include "Board.h"
#include "barrier.h"
   
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
   
static int mem_fd = -1;
static void *gpio_map;
static int clk_fd = -1;
static void *clk_map;
   
// I/O access
volatile unsigned *gpio;
volatile unsigned *gpio10;
volatile unsigned *gpio7;
volatile unsigned *gpio13;
volatile unsigned *gpio1;
volatile unsigned *gclk_base;
static volatile uint32_t *rp1Gpio;
static volatile uint32_t *rp1Clocks;

typedef enum {
	SOC_UNKNOWN = 0,
	SOC_BCM283X,   /* Pi 1/2/3/Zero and Pi 4/400 (BCM2835/6/7/2711) */
	SOC_RP1        /* Pi 5 (BCM2712 + RP1 southbridge) */
} SocType;

static SocType currentSoc = SOC_UNKNOWN;
static bool gclk_initialized = false;  /* Add this */

#define RP1_MEM_SIZE           0x30000
#define RP1_REG_SIZE           sizeof(uint32_t)
#define RP1_IO_BANK0_OFFSET    0x00000
#define RP1_SYS_RIO0_OFFSET    0x10000
#define RP1_PADS_BANK0_OFFSET  0x20000
#define RP1_RW_OFFSET          0x0000
#define RP1_SET_OFFSET         0x2000
#define RP1_CLR_OFFSET         0x3000
#define RP1_GPIO_CTRL          0x0004
#define RP1_GPIO_OFFSET        8
#define RP1_CTRL_FUNCSEL_MASK  0x001f
#define RP1_CTRL_OUTOVER_MASK  0x3000
#define RP1_CTRL_OEOVER_MASK   0xc000
#define RP1_FUNCSEL_GPIO       5
#define RP1_FUNCSEL_GPCLK0     3
#define RP1_PADS_GPIO          0x04
#define RP1_PADS_OFFSET        4
#define RP1_PADS_IN_ENABLE     0x40
#define RP1_PADS_OUT_DISABLE   0x80
#define RP1_PADS_BIAS_MASK     0x0c
#define RP1_PADS_BIAS_PULL_UP  0x08
#define RP1_RIO_OUT            0x00
#define RP1_RIO_OE             0x04
#define RP1_RIO_IN             0x08

#define RP1_CLOCK_BASE_PHYS    0x1f00018000ULL
#define RP1_CLOCK_MEM_SIZE     0x1000
#define RP1_GPCLK0_RATE        3579545ULL
#define RP1_XOSC_RATE          50000000ULL
#define RP1_GPCLK_OE_CTRL      0x00000
#define RP1_CLK_GP0_OFFSET     0x00174
#define RP1_CLK_GP0_CTRL       (RP1_CLK_GP0_OFFSET + 0x00)
#define RP1_CLK_GP0_DIV_INT    (RP1_CLK_GP0_OFFSET + 0x04)
#define RP1_CLK_GP0_DIV_FRAC   (RP1_CLK_GP0_OFFSET + 0x08)
#define RP1_CLK_GP0_SEL        (RP1_CLK_GP0_OFFSET + 0x0c)
#define RP1_CLK_CTRL_ENABLE    (1u << 11)
#define RP1_CLK_CTRL_AUXSRC_MASK (0x1fu << 5)
#define RP1_CLK_DIV_FRAC_BITS  16

/* BCM283x (Pi 3/4) physical register bases */
#define BCM_GPIO_BASE_PHYS     0x3F200000ULL
#define BCM_CLOCK_BASE_PHYS    0x3F101000ULL
#define BCM_CLOCK_MEM_SIZE     0x1000
#define BCM_GPFSEL0_OFFSET     0x00
#define BCM_GPSET0_OFFSET      0x1C
#define BCM_GPCLR0_OFFSET      0x28
#define BCM_GPLEV0_OFFSET      0x34
#define BCM_GPCLK0_CNTL        0x70
#define BCM_GPCLK0_DIV         0x74
#define BCM_GPCLK0_PASSWORD    0x5A000000

static SocType bcmDetectSoc(void)
{
	const char *compat = "/proc/device-tree/compatible";
	char buf[256];
	int fd = open(compat, O_RDONLY);
	if (fd < 0) {
		return SOC_BCM283X; /* assume the older /dev/mem style part */
	}
	int n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0) {
		return SOC_BCM283X;
	}
	buf[n] = '\0';
	if (strstr(buf, "brcm,bcm2712")) {
		return SOC_RP1;
	}
	return SOC_BCM283X;
}

static inline volatile uint32_t *rp1Reg(size_t offset)
{
	return rp1Gpio + (offset / RP1_REG_SIZE);
}

static inline volatile uint32_t *rp1ClockReg(size_t offset)
{
	return rp1Clocks + (offset / 4);
}

static void rp1SetGpioFunction(int pin, uint32_t function)
{
	volatile uint32_t *ctrl = rp1Reg(RP1_IO_BANK0_OFFSET + RP1_GPIO_CTRL + pin * RP1_GPIO_OFFSET + RP1_RW_OFFSET);
	uint32_t value = *ctrl;
	value &= ~(RP1_CTRL_FUNCSEL_MASK | RP1_CTRL_OUTOVER_MASK | RP1_CTRL_OEOVER_MASK);
	value |= function;
	*ctrl = value;
}

static void rp1EnablePad(int pin, int pullUp)
{
	volatile uint32_t *pad = rp1Reg(RP1_PADS_BANK0_OFFSET + RP1_PADS_GPIO + pin * RP1_PADS_OFFSET + RP1_RW_OFFSET);
	uint32_t value = *pad;
	value |= RP1_PADS_IN_ENABLE;
	value &= ~RP1_PADS_OUT_DISABLE;
	value &= ~RP1_PADS_BIAS_MASK;
	if (pullUp) {
		value |= RP1_PADS_BIAS_PULL_UP;
	}
	*pad = value;
}

static void rp1SetInput(int pin)
{
	rp1SetGpioFunction(pin, RP1_FUNCSEL_GPIO);
	rp1EnablePad(pin, 1);
	*rp1Reg(RP1_SYS_RIO0_OFFSET + RP1_RIO_OE + RP1_CLR_OFFSET) = 1u << pin;
}

static void rp1SetOutput(int pin)
{
	rp1SetGpioFunction(pin, RP1_FUNCSEL_GPIO);
	rp1EnablePad(pin, 1);
	*rp1Reg(RP1_SYS_RIO0_OFFSET + RP1_RIO_OE + RP1_SET_OFFSET) = 1u << pin;
}

/* BCM283x GPIO helpers */
static void bcmSetFunction(int pin, int function)
{
	volatile unsigned *fsel = gpio + (pin / 10);
	unsigned reg = *fsel;
	reg &= ~(7u << ((pin % 10) * 3));
	reg |= (unsigned)function << ((pin % 10) * 3);
	/* Group clock bank 0 uses GPFSEL0/1; bank 1 uses GPFSEL2/3... handled by gpio+ */
	*fsel = reg;
}

static void bcmSetInput(int pin)
{
	bcmSetFunction(pin, 0);
}

static void bcmSetOutput(int pin)
{
	bcmSetFunction(pin, 1);
}

/* GPIO setup macros, dispatch on the detected SoC */
#define INP_GPIO(g) ((currentSoc == SOC_RP1) ? rp1SetInput(g) : bcmSetInput(g))
#define OUT_GPIO(g) ((currentSoc == SOC_RP1) ? rp1SetOutput(g) : bcmSetOutput(g))
#define SET_GPIO_ALT(g,a) ((currentSoc == SOC_RP1) ? rp1SetGpioFunction((g),(a)) : bcmSetFunction((g),(a)))

#define GPIO_SET *(gpio7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio10) // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
#define GPIO (*(gpio13))

#define GZ_CLK_BUSY    (1 << 7)

/* Clock register pointers */
static volatile unsigned *bcm_gpclk0_ctl = NULL;
static volatile unsigned *bcm_gpclk0_div = NULL;

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
//#define MSX_CS12 (1 << CS12_PIN)
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
static int setup_gclk(void);
static void clear_gclk(void);
static void frontledWrite(unsigned char byte, int force);
void frontled(unsigned char byte);
int msxread(int slot, unsigned short addr);
void msxwrite(int slot, unsigned short addr, unsigned char byte);
int msxreadio(unsigned short addr);
void msxwriteio(unsigned short addr, unsigned char byte);
void clear_io();


/* Clock initialization state */
static bool gclk_initialized = false;

static int setup_gclk(void)
{
	uint64_t div;
	uint32_t divInt;
	uint32_t divFrac;
	uint32_t ctrl;
	uint64_t actualRate;

	// Already initialized, return early
	if (gclk_initialized) {
		return 0;
	}

	if (currentSoc == SOC_RP1) {
		/* RP1 (Pi 5) clock path */
		if (clk_map != NULL) {
			rp1SetGpioFunction(CLK_PIN, RP1_FUNCSEL_GPCLK0);
			rp1EnablePad(CLK_PIN, 0);
			gclk_initialized = true;
			return 0;
		}

		clk_fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
		if (clk_fd < 0) {
			fprintf(stderr, "Failed to open /dev/mem for RP1 GPCLK0: %s\n", strerror(errno));
			return -1;
		}

		clk_map = mmap(NULL, RP1_CLOCK_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, clk_fd, (off_t)RP1_CLOCK_BASE_PHYS);
		if (clk_map == MAP_FAILED) {
			int savedErrno = errno;
			fprintf(stderr, "Failed to map RP1 clock manager: %s\n", strerror(errno));
			close(clk_fd);
			clk_fd = -1;
			clk_map = NULL;
			errno = savedErrno;
			return -1;
		}

		rp1Clocks = (volatile uint32_t *)clk_map;
		div = ((RP1_XOSC_RATE << RP1_CLK_DIV_FRAC_BITS) + (RP1_GPCLK0_RATE / 2)) / RP1_GPCLK0_RATE;
		divInt = div >> RP1_CLK_DIV_FRAC_BITS;
		divFrac = (uint32_t)(div << (32 - RP1_CLK_DIV_FRAC_BITS));

		*rp1ClockReg(RP1_CLK_GP0_DIV_INT) = divInt;
		*rp1ClockReg(RP1_CLK_GP0_DIV_FRAC) = divFrac;
		*rp1ClockReg(RP1_CLK_GP0_SEL) = 1u;
		ctrl = *rp1ClockReg(RP1_CLK_GP0_CTRL);
		ctrl &= ~RP1_CLK_CTRL_AUXSRC_MASK;
		ctrl |= RP1_CLK_CTRL_ENABLE;
		*rp1ClockReg(RP1_CLK_GP0_CTRL) = ctrl;
		*rp1ClockReg(RP1_GPCLK_OE_CTRL) = *rp1ClockReg(RP1_GPCLK_OE_CTRL) | 1u;

		rp1SetGpioFunction(CLK_PIN, RP1_FUNCSEL_GPCLK0);
		rp1EnablePad(CLK_PIN, 0);

		actualRate = (RP1_XOSC_RATE << RP1_CLK_DIV_FRAC_BITS) / div;
		fprintf(stderr, "RP1 GPCLK0 enabled on GPIO20: requested %llu Hz, actual %llu Hz\n",
		        (unsigned long long)RP1_GPCLK0_RATE, (unsigned long long)actualRate);
		gclk_initialized = true;
		return 0;
	}

	/* BCM283x (Pi 3/4) clock path */
	if (clk_map == NULL) {
		fprintf(stderr, "Clock setup error: clk_map not initialized\n");
		return -1;
	}

	// Initialize clock pointers if needed
	if (gclk_base == NULL) {
		gclk_base = (volatile unsigned *)clk_map;
		bcm_gpclk0_ctl = (volatile unsigned *)((unsigned)gclk_base + (BCM_GPCLK0_CNTL / 4));
		bcm_gpclk0_div = (volatile unsigned *)((unsigned)gclk_base + (BCM_GPCLK0_DIV / 4));
	}

	if (bcm_gpclk0_ctl == NULL || bcm_gpclk0_div == NULL) {
		fprintf(stderr, "Clock initialization error: GPCLK pointer not set\n");
		return -1;
	}

	/* Set up BCM clock */
	div = ((19200000ULL << 12) + (RP1_GPCLK0_RATE / 2)) / RP1_GPCLK0_RATE;

	*bcm_gpclk0_div = (unsigned)(BCM_GPCLK0_PASSWORD | ((div >> 12) << 12) | (div & 0xfff));
	ctrl = *bcm_gpclk0_ctl;
	ctrl = (ctrl & ~0xff) | BCM_GPCLK0_PASSWORD | 6; /* source: 19.2 MHz XOSC */
	*bcm_gpclk0_ctl = (ctrl & ~1u) | BCM_GPCLK0_PASSWORD; /* stop first */
	*bcm_gpclk0_ctl = (ctrl | BCM_GPCLK0_PASSWORD) | 1u;  /* start */

	SET_GPIO_ALT(CLK_PIN, 0); /* GPIO20 = GPCLK0 (ALT0) on BCM283x */

	actualRate = 19200000ULL * 256 / ((div >> 12) * 256);
	fprintf(stderr, "BCM GPCLK0 enabled on GPIO20: requested %llu Hz (div=%llu)\n",
	        (unsigned long long)RP1_GPCLK0_RATE, (unsigned long long)(div >> 12));

	gclk_initialized = true;

	return 0;
}

static void clear_gclk(void)
{
	if (currentSoc == SOC_RP1) {
		if (clk_map != NULL) {
			*rp1ClockReg(RP1_GPCLK_OE_CTRL) = *rp1ClockReg(RP1_GPCLK_OE_CTRL) & ~1u;
			*rp1ClockReg(RP1_CLK_GP0_CTRL) = *rp1ClockReg(RP1_CLK_GP0_CTRL) & ~RP1_CLK_CTRL_ENABLE;
		}
	} else {
		if (bcm_gpclk0_ctl != NULL) {
			*bcm_gpclk0_ctl = (*bcm_gpclk0_ctl & ~1u) | BCM_GPCLK0_PASSWORD; /* stop GPCLK0 */
		}
	}

	if (gpio_map != NULL) {
		(currentSoc == SOC_RP1) ? rp1SetInput(CLK_PIN) : bcmSetInput(CLK_PIN);
	}

	if (clk_map != NULL) {
		munmap(clk_map, currentSoc == SOC_RP1 ? RP1_CLOCK_MEM_SIZE : BCM_CLOCK_MEM_SIZE);
		clk_map = NULL;
		rp1Clocks = NULL;
		gclk_base = NULL;
		bcm_gpclk0_ctl = NULL;
		bcm_gpclk0_div = NULL;
	}
	if (clk_fd >= 0) {
		close(clk_fd);
		clk_fd = -1;
	}
}

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
	for(int i=0; i<j/2; i++)   // plaire
	    GPIO_SET = 0;
}

void SetData(int ioflag, int flag, int delay, unsigned char byte)
{
	GPIO_SET = byte;
	GPIO_CLR = flag | MSX_WR;
	GPIO_SET = ioflag | MSX_WR;
	GPIO_SET = ioflag | MSX_WR;
	GPIO_CLR = flag;
    SetDelay(5);
	GPIO_CLR = MSX_WR;
	while(!(GPIO & MSX_WAIT));
    SetDelay(delay);
	GPIO_SET = MSX_WR;
    SetDelay(2);
   	GPIO_SET = MSX_CONTROLS;
	GPIO_CLR = LE_C;

}   

unsigned char GetData(int flag, int rflag, int delay)
{
	unsigned char byte;
	GPIO_SET = DAT_DIR | 0xff;
	GPIO_CLR = flag;
    SetDelay(1);
	GPIO_CLR = rflag;
	while(!(GPIO & MSX_WAIT));
	SetDelay(delay);
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
	byte = GetData((slot == 0 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ, MSX_RD | cs1 | cs2, 30);
#ifdef DEBUG    
	printf("+%04x:%02xr\n", addr, byte);
#endif
	return byte;	 
 }

 void msxwrite(int slot, unsigned short addr, unsigned char byte)
 {
	SetAddress(addr);
	SetData(MSX_MREQ, (slot == 0 ? MSX_SLTSL1 : MSX_SLTSL3) | MSX_MREQ, 45, byte);
#ifdef DEBUG  
	printf("+%04x:%02xw\n", addr, byte);
#endif
	return;
 }

 int msxreadio(unsigned short addr)
 {
	unsigned char byte;
	SetAddress(addr);
	byte = GetData(MSX_IORQ, MSX_RD, 45);
#ifdef DEBUG      
	printf("-IO%02x:%02xr\n", addr, byte);
#endif
	return byte;	 
 }

 void msxwriteio(unsigned short addr, unsigned char byte)
    {
	SetAddress(addr);
	SetData(MSX_IORQ, MSX_IORQ, 55, byte);
#ifdef DEBUG      
	printf("-IO%02x:%02xw\n", addr, byte);
#endif
	return;
 }

void checkInt()
{
	if (!(GPIO & MSX_INT))
	{
		boardSetInt(0x10000);
	}
} 

//
// Set up a memory regions to access GPIO
//
int setup_io()
{
	int i, speed_id, divisor ;	

	// Detect platform early in setup
	currentSoc = bcmDetectSoc();

	// Open the master /dev/mem device
	if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
		fprintf(stderr, "Cannot open /dev/mem: %s\n", strerror(errno));
		return -1;
	}

	// Map GPIO registers
	gpio_map = mmap(
		NULL, 
		PAGE_SIZE,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		mem_fd,
		currentSoc == SOC_RP1 ? 0x1F000000ULL : BCM_GPIO_BASE_PHYS  // GPIO base address for Pi 5 or BCM283x
	);

	if (gpio_map == MAP_FAILED) {
		fprintf(stderr, "Cannot map GPIO registers: %s\n", strerror(errno));
		close(mem_fd);
		return -1;
	}

	// Map clock registers  
	clk_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (clk_fd < 0) {
		fprintf(stderr, "Cannot open /dev/mem for clocks: %s\n", strerror(errno));
		return -1;
	}

	clk_map = mmap(
		NULL,
		currentSoc == SOC_RP1 ? RP1_CLOCK_MEM_SIZE : BCM_CLOCK_MEM_SIZE,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		clk_fd,
		currentSoc == SOC_RP1 ? (off_t)RP1_CLOCK_BASE_PHYS : (off_t)BCM_CLOCK_BASE_PHYS
	);

	if (clk_map == MAP_FAILED) {
		fprintf(stderr, "Cannot map clock registers: %s\n", strerror(errno));
		close(clk_fd);
		return -1;
	}

	gpio = (volatile unsigned *)gpio_map;

	if (currentSoc == SOC_RP1) {
		/* RP1 GPIO path */
		rp1Gpio = (volatile uint32_t *)gpio_map;
		gpio7 = gpio + 7;
		gpio10 = gpio + 10;
		gpio13 = gpio + 13;
		gpio1 = gpio + 1;

		// Setup the GPIO pins directly using RP1 register-based approach
		for(i = 0; i < 27; i++)
		{
			if(i != 20) { // Skip GPIO 20 since it's used for clock - we'll use direct register control
				rp1SetInput(i);
				// Set pull-up resistors where applicable  
				rp1EnablePad(i, 1);
			}
		}
	} else {
		/* BCM283x GPIO path */
		gpio7 = gpio + (BCM_GPSET0_OFFSET / 4);
		gpio10 = gpio + (BCM_GPCLR0_OFFSET / 4);
		gpio13 = gpio + (BCM_GPLEV0_OFFSET / 4);
		gpio1 = gpio + 1; /* Not actually used but included for completeness */
		
		// Setup the GPIO pins directly using BCM283x register-based approach
		for(i = 0; i < 27; i++)
		{
			if(i != 20) { // Skip GPIO 20 since it's used for clock - we'll use direct register control
				bcmSetInput(i);
				/* Pull-up is not supported in the old GPIO set/clear register method */
			}
		}
	}

	// Setup our specific clocks that are needed for MSX operation
	setup_gclk();

	GPIO_SET = LE_C | MSX_CONTROLS | MSX_WAIT | MSX_INT;
	GPIO_SET = LE_A | LE_D;
	GPIO_CLR = LE_C | 0xffff;
	GPIO_CLR = LE_C;
	GPIO_CLR = MSX_RESET;
	for(i=0;i<2000000;i++);
	GPIO_SET = MSX_RESET;
	for(i=0;i<1000000;i++);

	return 0;
} // setup_io

void clear_io()
{
	// Currently empty, as GPIOs are maintained in their state
}

void msxinit()
{
	const struct sched_param priority = {1};
	sched_setscheduler(0, SCHED_FIFO, &priority);  
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