#ifndef _DMA_H_
#define _DMA_H_


#define DMA_TI_PERMAP_SHIFT			16

#define DMA_TI_DREQ_SPITX           (6<<DMA_TI_PERMAP_SHIFT)
#define DMA_TI_DREQ_SPIRX           (7<<DMA_TI_PERMAP_SHIFT)
#define DMA_TI_SRC_IGNORE           (1<<11)
#define DMA_TI_SRC_DREQ             (1<<10)
#define DMA_TI_SRC_WIDTH_128BIT     (1<<9)
#define DMA_TI_SRC_INC              (1<<8)
#define DMA_TI_DEST_IGNORE          (1<<7)
#define DMA_TI_DEST_DREQ            (1<<6)
#define DMA_TI_DEST_WIDTH_128BIT    (1<<5)
#define DMA_TI_DEST_INC             (1<<4)
#define DMA_TI_WAIT_RESP            (1<<3)
#define DMA_TI_2DMODE               (1<<1)
#define DMA_TI_INTEN                (1<<0)

#define DMA_CS_INT                  (1<<2)
#define DMA_CS_END                  (1<<1)
#define DMA_CS_ACTIVE               (1<<0)

#define GPU_IO_BASE                 0x7E000000

#define BUS_IO_ADDRESS(addr)	    (unsigned int *)(((unsigned int)(addr) & ~0xFF000000) | GPU_IO_BASE)

void dma_init(unsigned int channel);
int dma_enqueue_operation( unsigned int channel, unsigned int* src, unsigned int *dst, unsigned int len, unsigned int stride, unsigned int TRANSFER_INFO );   
void dma_execute_queue(unsigned int channel);
void dma_memcpy_32( unsigned int channel, unsigned int* src, unsigned int *dst, unsigned int size );
int dma_running(unsigned int channel);

#endif
