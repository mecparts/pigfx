#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_



typedef enum {
FB_SUCCESS                = 0x0,
FB_ERROR                  = 0x1,
FB_POSTMAN_FAIL           = 0x2,
FB_GET_DISPLAY_SIZE_FAIL  = 0x3,
FB_FRAMEBUFFER_SETUP_FAIL = 0x4,
FB_INVALID_TAG_DATA       = 0x5,
FB_INVALID_PITCH          = 0x6
} FB_RETURN_TYPE;


extern unsigned int get_rgb(unsigned char i);
extern FB_RETURN_TYPE fb_init( unsigned int ph_w, unsigned int ph_h, unsigned int vrt_w, unsigned int vrt_h,
                               unsigned int bpp, void** pp_fb, unsigned int* pfbsize, unsigned int* pPitch );
extern FB_RETURN_TYPE fb_release();
extern FB_RETURN_TYPE fb_set_grayscale_palette();
extern FB_RETURN_TYPE fb_set_xterm_palette();
extern FB_RETURN_TYPE fb_blank_screen( unsigned int blank );
extern FB_RETURN_TYPE fb_get_pitch( unsigned int* pPitch );
extern FB_RETURN_TYPE fb_set_depth( unsigned int* pDepth );
extern FB_RETURN_TYPE fb_get_virtual_buffer_size( unsigned int* pVWidth, unsigned int* pVHeight );
extern FB_RETURN_TYPE fb_get_phisical_buffer_size( unsigned int* pWidth, unsigned int* pHeight );
extern FB_RETURN_TYPE fb_set_phisical_buffer_size( unsigned int* pWidth, unsigned int* pHeight );
extern FB_RETURN_TYPE fb_set_virtual_offset( unsigned int pX, unsigned int pY );
extern FB_RETURN_TYPE fb_set_virtual_buffer_size( unsigned int* pWidth, unsigned int* pHeight );
extern FB_RETURN_TYPE fb_allocate_buffer( void** ppBuffer, unsigned int* pBufferSize );


#endif
