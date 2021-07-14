#include "dma.h"
#include "utils.h"

#define DMA_CHANNELS 3
#define DMA_SCBS 4
#define DMA_BASE 0x20007000
#define DMA_GLOBAL_ENABLE   (DMA_BASE + 0x0FF0)
#define DMA_CS_OFFSET        0x00
#define DMA_CONBLK_AD_OFFSET 0x04
// https://www.raspberrypi.org/forums/viewtopic.php?f=72&t=10276


typedef struct _DMA_Ctrl_Block
{
    unsigned int TI;                    // Transfer information
    unsigned int SOURCE_AD;             // source address 
    unsigned int DEST_AD;               // destination address 
    unsigned int TXFR_LEN;              // transfer length 
    unsigned int STRIDE;                // 2D mode stride
    struct _DMA_Ctrl_Block* NEXTCONBK;  // Next control block address 
    unsigned int reserved1;
    unsigned int reserved2;

} DMA_Control_Block;                    // 256 bits long exactly


// DMA control blocks must be aligned on 256 bit boundaries (32 bytes)
DMA_Control_Block __attribute__((aligned(0x100))) ctr_blocks[DMA_CHANNELS][DMA_SCBS];
unsigned int curr_blk[DMA_CHANNELS];


void dma_init(unsigned int channel)
{
    if( channel < DMA_CHANNELS ) {
        curr_blk[channel] = 0;

        // Enable DMA on channel
        W32(DMA_GLOBAL_ENABLE, R32(DMA_GLOBAL_ENABLE) | (1 << channel));
    }
}


int dma_enqueue_operation( unsigned int channel, unsigned int* src, unsigned int *dst, unsigned int len, unsigned int stride, unsigned int TRANSFER_INFO )
{
    if( channel >= DMA_CHANNELS ) {
        return 0;
    }
    if( curr_blk[channel] == DMA_SCBS ) {
        return 0;
    }

    DMA_Control_Block* blk = (DMA_Control_Block*)mem_2uncached( &( ctr_blocks[ channel ][ curr_blk[channel] ]) );
    blk->TI = TRANSFER_INFO;
    blk->SOURCE_AD = (unsigned int)src;
    blk->DEST_AD = (unsigned int)dst;
    blk->TXFR_LEN = len;
    blk->STRIDE = stride;
    blk->NEXTCONBK = 0;
    blk->reserved1 = 0;
    blk->reserved2 = 0;

    if( curr_blk[channel] > 0 )
    {
        // Enqueue 
        ctr_blocks[ channel ][ curr_blk[channel] -1 ].NEXTCONBK = blk;
    }

    ++curr_blk[channel];
    return curr_blk[channel];
}  


void dma_execute_queue(unsigned int channel)
{
    if( channel < DMA_CHANNELS ) {
        peripheral_entry();

        // Set the first control block
        W32(DMA_BASE + (channel << 8) + DMA_CONBLK_AD_OFFSET, 
            mem_2uncached( &(ctr_blocks[channel][0]) ));

        // Start the operation
        W32(DMA_BASE + (channel << 8) + DMA_CS_OFFSET, 
            DMA_CS_INT | DMA_CS_END | DMA_CS_ACTIVE);

        // reset the queue
        curr_blk[ channel ] = 0;
    
        peripheral_exit();
    }
}


int dma_running(unsigned int channel)
{
    int running = 0;

    if( channel < DMA_CHANNELS ) {
        peripheral_entry();
    
        running = R32(DMA_BASE + (channel << 8) + DMA_CS_OFFSET) & DMA_CS_ACTIVE;

        peripheral_exit();
    }
    return running;
}


void dma_memcpy_32( unsigned int channel, unsigned int* src, unsigned int *dst, unsigned int size )
{
    if( channel < DMA_CHANNELS ) {
        dma_enqueue_operation( channel, src, dst, size, 0, DMA_TI_SRC_INC | DMA_TI_DEST_INC ); 
        dma_execute_queue(channel);
    }
}


