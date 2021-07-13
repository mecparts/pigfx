#ifndef _PIGFX_UTILS_H_
#define _PIGFX_UTILS_H_

extern void enable_irq();
extern void disable_irq();

extern void busywait( unsigned int cycles );
extern void W32( unsigned int addr, unsigned int data );
extern unsigned int R32( unsigned int addr );
extern void membarrier();
extern void peripheral_entry();
extern void peripheral_exit();

#define GPIO_INPUT  0
#define GPIO_OUTPUT 1
#define GPIO_ALT0   4
#define GPIO_ALT1   5
#define GPIO_ALT2   6
#define GPIO_ALT3   7
#define GPIO_ALT4   3
#define GPIO_ALT5   2

extern void pinMode( unsigned int pin, unsigned int mode );

#define LOW     0
#define HIGH    1

extern void digitalWrite( unsigned int pin, unsigned int state);

/**
 * String related
 */
extern unsigned int hex2byte( unsigned char* addr );
extern void byte2hexstr( unsigned char byte, char* outstr );
extern void word2hexstr( unsigned int word, char* outstr );
extern unsigned int strlen( char* str );
extern int strcmp( char*s1, char* s2 );

/**
 * Memory
 */
/*
inline void memcpy( unsigned char* dst, unsigned char* src, unsigned int len )
{
    unsigned char* srcEnd = src + len;
    while( src<srcEnd )
    {
        *dst++ =  *src++;
    }
}
*/

/*
 *   Data memory barrier
 *   No memory access after the DMB can run until all memory accesses before it
 *    have completed
 *    
 */
#define dmb() asm volatile \
                ("mcr p15, #0, %[zero], c7, c10, #5" : : [zero] "r" (0) )


/*
 *  Data synchronisation barrier
 *  No instruction after the DSB can run until all instructions before it have
 *  completed
 */
#define dsb() asm volatile \
                ("mcr p15, #0, %[zero], c7, c10, #4" : : [zero] "r" (0) )


/*
 * Clean and invalidate entire cache
 * Flush pending writes to main memory
 * Remove all data in data cache
 */
#define flushcache() asm volatile \
                ("mcr p15, #0, %[zero], c7, c14, #0" : : [zero] "r" (0) )



#define mem_p2v(X) (X)
#define mem_v2p(X) (X)
//#define mem_2uncached(X) (X)
//#define mem_2cached(X)   (X)

//#define mem_p2v(X) ((((unsigned int)X)&0x0FFFFFFF)|0x40000000)
//#define mem_v2p(X) ((((unsigned int)X)&0x0FFFFFFF))
#define mem_2uncached(X) ((((unsigned int)X)&0x0FFFFFFF)|0x40000000)
#define mem_2cached(X)   ((((unsigned int)X)&0x0FFFFFFF))





#endif
