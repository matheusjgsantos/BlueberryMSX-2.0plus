#ifndef BARRIER_H
#define BARRIER_H

/*
 * Data memory barrier
 * No memory access after the DMB can run until all memory accesses before it
 * have completed
 */
#ifdef RPI3
#define dmb() asm volatile \
		("DMB ISHST");
#else
#define dmb() asm volatile \
		("mcr p15, #0, %[zero], c7, c5, #0" : : [zero] "r" (0) ); \
		asm volatile \
		("mcr p15, #0, %[zero], c7, c6, #0" : : [zero] "r" (0) );
#endif

#define isb0() __asm__ __volatile__ ("mcr p15, 0, %0, c7,  c5, 4" : : "r" (0) : "memory")
#define dmb0() __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory")
//ARMv6 DSB (DataSynchronizationBarrier): also known as DWB (drain write buffer / data write barrier) on ARMv5
#define dsb0() __asm__ __volatile__ ("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory")

/*
 * Data synchronisation barrier
 * No instruction after the DSB can run until all instructions before it have
 * completed
 */
#define dsb() asm volatile \
		("mcr p15, #0, %[zero], c7, c10, #1" : : [zero] "r" (0) )

#define invmva() asm volatile \
		("mcr p15, #0, %[zero], c7, c10, #1" : : [zero] "r" (0) )

#define invset() asm volatile \
		("mcr p15, #0, %[zero], c7, c10, #2" : : [zero] "r" (0) )
/*
 * Clean and invalidate entire cache
 * Flush pending writes to main memory
 * Remove all data in data cache
 */
#define flushcache() asm volatile \
		("mcr p15, #0, %[zero], c7, c14, #0" : : [zero] "r" (0) )

#endif	/* BARRIER_H */