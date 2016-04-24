#ifndef __MSXBUS__
#define __MSXBUS__
int msxread(int slot, unsigned short addr);
void msxwrite(int slot, unsigned short addr, unsigned char byte);
int msxreadio(unsigned short addr);
void msxwriteio(unsigned short addr, unsigned char byte);
int msxinit();
int msxclose();
#endif