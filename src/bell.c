//
// bell.c: using the MOSI output of SPI0, send 200ms of 800Hz.
// This is accomplished by setting the SPI0 clock to 8000Hz, and
// sending 5 bits of 1s followed by 5 bits of 0s for a complete
// 800Hz cycle. 160 such cycles are needed for 200ms of tone. That
// works out to 160 * 10 / 8 = 200 bytes of data to be sent.
// 
// DMA is used so that the bell is "fire and forget" to the PiGFX
// code. Yes; it's like using a chainsaw to cut a roast; way too much
// firepower for such a simple little task. But the SPI was sitting
// there unused, and so were 15 of the 16 DMA channels in the BCM2835,
// so... why not?
//
// Only the MISO pin is actually used, so none of the other usual SPI
// pins (CLK, MISO, CSx) are defined. Honestly, I was a little
// surprised that that actually worked.
//
// V0.9  Jul 11 2021  SWH  First non DMA version working
//
#include "utils.h"

#define CORE_CLOCK 250000000ul
#define SPI_CLOCK 8000

#define SPI0_BASE 0x20204000

#define SPI0_CS     (SPI0_BASE + 0x00)
#define SPI0_FIFO   (SPI0_BASE + 0x04)
#define SPI0_CLK    (SPI0_BASE + 0x08)
#define SPI0_DLEN   (SPI0_BASE + 0x0c)
#define SPI0_DC     (SPI0_BASE + 0x14)

// CS Register
#define CS_LEN_LONG	(1 << 25)
#define CS_DMA_LEN	(1 << 24)
#define CS_CSPOL2	(1 << 23)
#define CS_CSPOL1	(1 << 22)
#define CS_CSPOL0	(1 << 21)
#define CS_RXF		(1 << 20)
#define CS_RXR		(1 << 19)
#define CS_TXD		(1 << 18)
#define CS_RXD		(1 << 17)
#define CS_DONE		(1 << 16)
#define CS_LEN		(1 << 13)
#define CS_REN		(1 << 12)
#define CS_ADCS		(1 << 11)
#define CS_INTR		(1 << 10)
#define CS_INTD		(1 << 9)
#define CS_DMAEN	(1 << 8)
#define CS_TA		(1 << 7)
#define CS_CSPOL	(1 << 6)
#define CS_CLEAR_RX	(1 << 5)
#define CS_CLEAR_TX	(1 << 4)
#define CS_CPOL__SHIFT	3
#define CS_CPHA__SHIFT	2
#define CS_CS		(1 << 0)
#define CS_CS__SHIFT	0

void bell_init() {
    pinMode(10, GPIO_ALT0); // GPIO10 is SPI0 MOSI
    
    peripheral_entry();

    // set SPI clock to 8000Hz
	W32(SPI0_CLK, CORE_CLOCK / SPI_CLOCK);
	W32(SPI0_CS, (0 << CS_CPOL__SHIFT) | (0 << CS_CPHA__SHIFT));
    
    peripheral_exit();
}

void bell() {
    // console bell: 200ms of 800Hz tone (160 x 10 bit cycles)
    static const unsigned char bell_bits[] = {
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000,
        0b11111000,0b00111110,0b00001111,0b10000011,0b11100000
    };
	const unsigned char *pWritePtr = bell_bits;
    unsigned int nCount = sizeof bell_bits/sizeof(const unsigned char);
    unsigned int nData;

	peripheral_entry ();

	// SCLK stays low for one clock cycle after each byte without this
	W32(SPI0_DLEN, nCount);

	W32(SPI0_CS, R32(SPI0_CS) | CS_CLEAR_RX | CS_CLEAR_TX | CS_TA);

	unsigned int nWriteCount = 0;
	unsigned int nReadCount = 0;

	while( nWriteCount < nCount || nReadCount  < nCount) {
		while( nWriteCount < nCount && (R32(SPI0_CS) & CS_TXD)) {
			nData = *pWritePtr++;
			W32(SPI0_FIFO, nData);
            ++nWriteCount;
		}

		while( nReadCount < nCount && (R32(SPI0_CS) & CS_RXD) ) {
			R32(SPI0_FIFO);
			++nReadCount;
		}
	}

	while( !(R32(SPI0_CS) & CS_DONE) ) {
		while( R32(SPI0_CS) & CS_RXD ) {
			R32(SPI0_FIFO);
		}
	}

	peripheral_exit();
}
