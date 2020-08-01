#include "pigfx_config.h"
#include "framebuffer.h"
#include "postman.h"
#include "console.h"
#include "utils.h"


static const unsigned int xterm_colors[256] = {
0x000000,  0x800000,  0x008000,  0x808000,  0x000080,
0x800080,  0x008080,  0xc0c0c0,  0x808080,  0xff0000,
0x00ff00,  0xffff00,  0x0000ff,  0xff00ff,  0x00ffff,
0xffffff,  0x000000,  0x00005f,  0x000087,  0x0000af,
0x0000d7,  0x0000ff,  0x005f00,  0x005f5f,  0x005f87,
0x005faf,  0x005fd7,  0x005fff,  0x008700,  0x00875f,
0x008787,  0x0087af,  0x0087d7,  0x0087ff,  0x00af00,
0x00af5f,  0x00af87,  0x00afaf,  0x00afd7,  0x00afff,
0x00d700,  0x00d75f,  0x00d787,  0x00d7af,  0x00d7d7,
0x00d7ff,  0x00ff00,  0x00ff5f,  0x00ff87,  0x00ffaf,
0x00ffd7,  0x00ffff,  0x5f0000,  0x5f005f,  0x5f0087,
0x5f00af,  0x5f00d7,  0x5f00ff,  0x5f5f00,  0x5f5f5f,
0x5f5f87,  0x5f5faf,  0x5f5fd7,  0x5f5fff,  0x5f8700,
0x5f875f,  0x5f8787,  0x5f87af,  0x5f87d7,  0x5f87ff,
0x5faf00,  0x5faf5f,  0x5faf87,  0x5fafaf,  0x5fafd7,
0x5fafff,  0x5fd700,  0x5fd75f,  0x5fd787,  0x5fd7af,
0x5fd7d7,  0x5fd7ff,  0x5fff00,  0x5fff5f,  0x5fff87,
0x5fffaf,  0x5fffd7,  0x5fffff,  0x870000,  0x87005f,
0x870087,  0x8700af,  0x8700d7,  0x8700ff,  0x875f00,
0x875f5f,  0x875f87,  0x875faf,  0x875fd7,  0x875fff,
0x878700,  0x87875f,  0x878787,  0x8787af,  0x8787d7,
0x8787ff,  0x87af00,  0x87af5f,  0x87af87,  0x87afaf,
0x87afd7,  0x87afff,  0x87d700,  0x87d75f,  0x87d787,
0x87d7af,  0x87d7d7,  0x87d7ff,  0x87ff00,  0x87ff5f,
0x87ff87,  0x87ffaf,  0x87ffd7,  0x87ffff,  0xaf0000,
0xaf005f,  0xaf0087,  0xaf00af,  0xaf00d7,  0xaf00ff,
0xaf5f00,  0xaf5f5f,  0xaf5f87,  0xaf5faf,  0xaf5fd7,
0xaf5fff,  0xaf8700,  0xaf875f,  0xaf8787,  0xaf87af,
0xaf87d7,  0xaf87ff,  0xafaf00,  0xafaf5f,  0xafaf87,
0xafafaf,  0xafafd7,  0xafafff,  0xafd700,  0xafd75f,
0xafd787,  0xafd7af,  0xafd7d7,  0xafd7ff,  0xafff00,
0xafff5f,  0xafff87,  0xafffaf,  0xafffd7,  0xafffff,
0xd70000,  0xd7005f,  0xd70087,  0xd700af,  0xd700d7,
0xd700ff,  0xd75f00,  0xd75f5f,  0xd75f87,  0xd75faf,
0xd75fd7,  0xd75fff,  0xd78700,  0xd7875f,  0xd78787,
0xd787af,  0xd787d7,  0xd787ff,  0xd7af00,  0xd7af5f,
0xd7af87,  0xd7afaf,  0xd7afd7,  0xd7afff,  0xd7d700,
0xd7d75f,  0xd7d787,  0xd7d7af,  0xd7d7d7,  0xd7d7ff,
0xd7ff00,  0xd7ff5f,  0xd7ff87,  0xd7ffaf,  0xd7ffd7,
0xd7ffff,  0xff0000,  0xff005f,  0xff0087,  0xff00af,
0xff00d7,  0xff00ff,  0xff5f00,  0xff5f5f,  0xff5f87,
0xff5faf,  0xff5fd7,  0xff5fff,  0xff8700,  0xff875f,
0xff8787,  0xff87af,  0xff87d7,  0xff87ff,  0xffaf00,
0xffaf5f,  0xffaf87,  0xffafaf,  0xffafd7,  0xffafff,
0xffd700,  0xffd75f,  0xffd787,  0xffd7af,  0xffd7d7,
0xffd7ff,  0xffff00,  0xffff5f,  0xffff87,  0xffffaf,
0xffffd7,  0xffffff,  0x080808,  0x121212,  0x1c1c1c,
0x262626,  0x303030,  0x3a3a3a,  0x444444,  0x4e4e4e,
0x585858,  0x626262,  0x6c6c6c,  0x767676,  0x808080,
0x8a8a8a,  0x949494,  0x9e9e9e,  0xa8a8a8,  0xb2b2b2,
0xbcbcbc,  0xc6c6c6,  0xd0d0d0,  0xdadada,  0xe4e4e4,
0xeeeeee 
};

static const unsigned char xored_clr_index[256] = {
   15,123,213,105,228,120,210,237,
    8, 14, 13, 12, 11, 10,  9,  0,
   15,229,228,227, 11, 11,219,217,
  216,215,214,214,213,211,210,209,
  208,208,207,205,204,203,202,202,
   13,199,198,197,  9,  9, 13,199,
  198,197,  9,  9,159,157,156,155,
  154,154,147,247,247,143,142,142,
  141,247,138,137,136,136,135,133,
  132,131,130,130,129,127,126,125,
  124,124,129,127,126,125,124,124,
  123,121,120,119,118,118,111,247,
  108,107,106,106,105,103,243,243,
    3,  3, 99, 97,243, 95, 94, 94,
   93, 91,  5, 89,  1,  1, 93, 91,
    5, 89,  1,  1, 87, 85, 84, 83,
   82, 82, 75, 73, 72, 71, 70, 70,
   69, 67,243, 65, 64, 64, 63, 61,
   60,239,239, 58, 57, 55, 54,239,
  235, 52, 57, 55, 54, 53, 52, 52,
   14, 49, 48, 47, 10, 10, 39, 37,
   36, 35, 34, 34, 33, 31,  6, 29,
    2,  2, 27, 25, 24,239,235, 22,
   12, 19,  4,235,234,234, 12, 19,
    4, 17,234,  0, 14, 49, 48, 47,
   10, 10, 39, 37, 36, 35, 34, 34,
   33, 31,  6, 29,  2,  2, 27, 25,
   24, 23, 22, 22, 12, 19,  4, 17,
  234,  0, 12, 19,  4, 17,  0,  0,
   15,255,254,253,252,251,250,249,
  248,247,246,245,  8,243,242,241,
  240,239,238,237,236,235,234,233
};

unsigned int get_rgb(unsigned char i) {
	return xterm_colors[i];
}

unsigned char get_xored_index(unsigned char i) {
    return xored_clr_index[i];
}

/*
 * Framebuffer initialization is a modifided version 
 * of the code originally written by brianwiddas.
 *
 * See:
 * https://github.com/brianwiddas/pi-baremetal.git
 *
 */
FB_RETURN_TYPE fb_init( unsigned int ph_w, unsigned int ph_h, unsigned int vrt_w, unsigned int vrt_h,
                        unsigned int bpp, void** pp_fb, unsigned int* pfbsize, unsigned int* pPitch )
{
    unsigned int var;
    unsigned int count;
    unsigned int physical_screenbase;

    /* Storage space for the buffer used to pass information between the
     * CPU and VideoCore
     * Needs to be aligned to 16 bytes as the bottom 4 bits of the address
     * passed to VideoCore are used for the mailbox number
     */
    volatile unsigned int mailbuffer[256] __attribute__((aligned (16)));

    /* Physical memory address of the mailbuffer, for passing to VC */
    unsigned int physical_mb = mem_v2p((unsigned int)mailbuffer);

    /* Get the display size */
    mailbuffer[0] = 8 * 4;      // Total size
    mailbuffer[1] = 0;          // Request
    mailbuffer[2] = 0x40003;    // Display size
    mailbuffer[3] = 8;          // Buffer size
    mailbuffer[4] = 0;          // Request size
    mailbuffer[5] = 0;          // Space for horizontal resolution
    mailbuffer[6] = 0;          // Space for vertical resolution
    mailbuffer[7] = 0;          // End tag

    if( POSTMAN_SUCCESS != postman_send( 8, physical_mb ) )
        return FB_POSTMAN_FAIL;

    if( POSTMAN_SUCCESS != postman_recv( 8, &var ) )
        return FB_POSTMAN_FAIL;

    /* Valid response in data structure */
    if(mailbuffer[1] != 0x80000000)
        return FB_GET_DISPLAY_SIZE_FAIL;


#if ENABLED(FRAMEBUFFER_DEBUG)
    unsigned int display_w = mailbuffer[5];
    unsigned int display_h = mailbuffer[6];
    cout("Display size: ");cout_d(display_w);cout("x");cout_d(display_h);cout_endl();
#endif

    /* Set up screen */
    unsigned int c = 1;
    mailbuffer[c++] = 0;        // Request

    mailbuffer[c++] = 0x00048003;   // Tag id (set physical size)
    mailbuffer[c++] = 8;        // Value buffer size (bytes)
    mailbuffer[c++] = 8;        // Req. + value length (bytes)
    mailbuffer[c++] = ph_w;     // Horizontal resolution
    mailbuffer[c++] = ph_h;     // Vertical resolution

    mailbuffer[c++] = 0x00048004;   // Tag id (set virtual size)
    mailbuffer[c++] = 8;        // Value buffer size (bytes)
    mailbuffer[c++] = 8;        // Req. + value length (bytes)
    mailbuffer[c++] = vrt_w;     // Horizontal resolution
    mailbuffer[c++] = vrt_h;     // Vertical resolution

    mailbuffer[c++] = 0x00048005;   // Tag id (set depth)
    mailbuffer[c++] = 4;        // Value buffer size (bytes)
    mailbuffer[c++] = 4;        // Req. + value length (bytes)
    mailbuffer[c++] = bpp;      //  bpp

    mailbuffer[c++] = 0x00040001;   // Tag id (allocate framebuffer)
    mailbuffer[c++] = 8;        // Value buffer size (bytes)
    mailbuffer[c++] = 4;        // Req. + value length (bytes)
    mailbuffer[c++] = 16;       // Alignment = 16
    mailbuffer[c++] = 0;        // Space for response

    mailbuffer[c++] = 0;        // Terminating tag

    mailbuffer[0] = c*4;        // Buffer size


    if( POSTMAN_SUCCESS != postman_send( 8, physical_mb ) )
        return FB_POSTMAN_FAIL;

    if( POSTMAN_SUCCESS != postman_recv( 8, &var ) )
        return FB_POSTMAN_FAIL;


    /* Valid response in data structure */
    if(mailbuffer[1] != 0x80000000)
        return FB_FRAMEBUFFER_SETUP_FAIL;

    count=2;    /* First tag */
    while((var = mailbuffer[count]))
    {
        if(var == 0x40001)
            break;

        /* Skip to next tag
         * Advance count by 1 (tag) + 2 (buffer size/value size)
         *                          + specified buffer size
         */
        count += 3+(mailbuffer[count+1]>>2);

        if(count>c)
            return FB_FRAMEBUFFER_SETUP_FAIL;
    }

    /* 8 bytes, plus MSB set to indicate a response */
    if(mailbuffer[count+2] != 0x80000008)
        return FB_FRAMEBUFFER_SETUP_FAIL;

    /* Framebuffer address/size in response */
    physical_screenbase = mailbuffer[count+3];
    *pfbsize = mailbuffer[count+4];

    if(physical_screenbase == 0 || *pfbsize == 0)
        return FB_INVALID_TAG_DATA;

    /* physical_screenbase is the address of the screen in RAM
     *   * screenbase needs to be the screen address in virtual memory
     *       */
    *pp_fb = (void*)mem_p2v(physical_screenbase);

#if ENABLED(FRAMEBUFFER_DEBUG)
    cout("Screen addr: ");cout_h((unsigned int)*pp_fb); cout_endl();
    cout("Screen size: ");cout_d(*pfbsize); cout_endl();
#endif

    /* Get the framebuffer pitch (bytes per line) */
    mailbuffer[0] = 7 * 4;      // Total size
    mailbuffer[1] = 0;      // Request
    mailbuffer[2] = 0x40008;    // Display size
    mailbuffer[3] = 4;      // Buffer size
    mailbuffer[4] = 0;      // Request size
    mailbuffer[5] = 0;      // Space for pitch
    mailbuffer[6] = 0;      // End tag

    if( POSTMAN_SUCCESS != postman_send( 8, physical_mb ) )
        return FB_POSTMAN_FAIL;

    if( POSTMAN_SUCCESS != postman_recv( 8, &var ) )
        return FB_POSTMAN_FAIL;

    /* 4 bytes, plus MSB set to indicate a response */
    if(mailbuffer[4] != 0x80000004)
        return FB_INVALID_PITCH;

    *pPitch = mailbuffer[5];

    if( *pPitch == 0 )
        return FB_INVALID_PITCH;

#if ENABLED(FRAMEBUFFER_DEBUG)
    cout("pitch: "); cout_d(*pPitch); cout_endl();
#endif

    return FB_SUCCESS;
}



FB_RETURN_TYPE fb_release()
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;


    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00048001;  // Tag: blank
    pBuffData[off++] = 0;           // response buffer size in bytes
    pBuffData[off++] = 0;           // request size
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    unsigned int physical_mb = mem_v2p((unsigned int)pBuffData);
    if( POSTMAN_SUCCESS != postman_send( 8, physical_mb ) )
        return FB_POSTMAN_FAIL;

    if( POSTMAN_SUCCESS != postman_recv( 8, &respmsg ) )
        return FB_POSTMAN_FAIL;

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    return FB_SUCCESS;

}


FB_RETURN_TYPE fb_set_grayscale_palette()
{
    volatile unsigned int pBuffData[4096] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x0004800B;  // Tag: blank
    pBuffData[off++] = 4;           // response buffer size in bytes
    pBuffData[off++] = 1032;        // request size
    pBuffData[off++] = 0;           // first palette index
    pBuffData[off++] = 256;         // num entries 

    unsigned int pi;
    for(pi=0;pi<256;++pi)
    {
        unsigned int entry = 0;

        entry =  (pi & 0xFF)<<16 | (pi & 0xFF)<<8 | (pi & 0xFF);
        entry = entry | 0xFF000000; //alpha

        pBuffData[off++] = entry;         
    }
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    unsigned int physical_mb = mem_v2p((unsigned int)pBuffData);
    if( POSTMAN_SUCCESS != postman_send( 8, physical_mb ) )
        return FB_POSTMAN_FAIL;

    if( POSTMAN_SUCCESS != postman_recv( 8, &respmsg ) )
        return FB_POSTMAN_FAIL;

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    return FB_SUCCESS;

}


FB_RETURN_TYPE fb_set_xterm_palette()
{
    volatile unsigned int pBuffData[4096] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x0004800B;  // Tag: blank
    pBuffData[off++] = 4;           // response buffer size in bytes
    pBuffData[off++] = 1032;        // request size
    pBuffData[off++] = 0;           // first palette index
    pBuffData[off++] = 256;         // num entries 

    unsigned int pi;
    for(pi=0;pi<256;++pi)
    {
        const unsigned int vc = xterm_colors[pi];
        // RGB -> BGR
        pBuffData[off++] = ((vc & 0xFF)<<16) | (vc & 0x00FF00) | ((vc>>16) & 0xFF) | 0xFF000000;         
    }
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    unsigned int physical_mb = mem_v2p((unsigned int)pBuffData);
    if( POSTMAN_SUCCESS != postman_send( 8, physical_mb ) )
        return FB_POSTMAN_FAIL;

    if( POSTMAN_SUCCESS != postman_recv( 8, &respmsg ) )
        return FB_POSTMAN_FAIL;

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    return FB_SUCCESS;
}


FB_RETURN_TYPE fb_blank_screen( unsigned int blank )
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;


    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00040002;  // Tag: blank
    pBuffData[off++] = 4;           // response buffer size in bytes
    pBuffData[off++] = 4;           // request size
    pBuffData[off++] = blank;       // state
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    unsigned int physical_mb = mem_v2p((unsigned int)pBuffData);
    if( POSTMAN_SUCCESS != postman_send( 8, physical_mb ) )
        return FB_POSTMAN_FAIL;

    if( POSTMAN_SUCCESS != postman_recv( 8, &respmsg ) )
        return FB_POSTMAN_FAIL;

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    return FB_SUCCESS;
}



FB_RETURN_TYPE fb_set_depth( unsigned int* pDepth )
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00048005;  // Tag: get pitch 
    pBuffData[off++] = 4;           // response buffer size in bytes
    pBuffData[off++] = 4;           // request size 
    pBuffData[off++] = *pDepth;           // response buffer
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    postman_send( 8, mem_v2p((unsigned int)pBuffData) );
    postman_recv( 8, &respmsg);

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    *pDepth = pBuffData[5];

    return FB_SUCCESS;
}



FB_RETURN_TYPE fb_get_pitch( unsigned int* pPitch )
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00040008;  // Tag: get pitch 
    pBuffData[off++] = 4;           // response buffer size in bytes
    pBuffData[off++] = 0;           // request size 
    pBuffData[off++] = 0;           // response buffer
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    postman_send( 8, mem_v2p((unsigned int)pBuffData) );
    postman_recv( 8, &respmsg);

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    if( pPitch )
        *pPitch = pBuffData[5];

    return FB_SUCCESS;
}


FB_RETURN_TYPE fb_get_phisical_buffer_size( unsigned int* pWidth, unsigned int* pHeight )
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00040003;  // Tag: get phisical buffer size 
    pBuffData[off++] = 8;           // response buffer size in bytes
    pBuffData[off++] = 0;           // request size 
    pBuffData[off++] = 0;           // response buffer
    pBuffData[off++] = 0;           // response buffer
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    postman_send( 8, mem_v2p((unsigned int)pBuffData) );
    postman_recv( 8, &respmsg);

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    if( pWidth )
        *pWidth = pBuffData[5];
    if( pHeight )
        *pHeight = pBuffData[6];

    return FB_SUCCESS;
}


FB_RETURN_TYPE fb_set_phisical_buffer_size( unsigned int* pWidth, unsigned int* pHeight )
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00048003;  // Tag: set phisical buffer size 
    pBuffData[off++] = 8;           // response buffer size in bytes
    pBuffData[off++] = 8;           // request size 
    pBuffData[off++] = *pWidth;     // response buffer
    pBuffData[off++] = *pHeight;    // response buffer
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    postman_send( 8, mem_v2p((unsigned int)pBuffData) );
    postman_recv( 8, &respmsg);

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    if( pWidth )
        *pWidth = pBuffData[5];
    if( pHeight )
        *pHeight = pBuffData[6];

    return FB_SUCCESS;
}


FB_RETURN_TYPE fb_allocate_buffer( void** ppBuffer, unsigned int* pBufferSize )
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00040004;  // Tag: allocate buffer 
    pBuffData[off++] = 8;           // response buffer size in bytes
    pBuffData[off++] = 4;           // request size 
    pBuffData[off++] = 16;          // response buffer
    pBuffData[off++] = 0;           // response buffer
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    postman_send( 8, mem_v2p((unsigned int)pBuffData) );
    postman_recv( 8, &respmsg);

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    *ppBuffer = (void*)pBuffData[5];
    *pBufferSize = pBuffData[6];

    return FB_SUCCESS;
}


FB_RETURN_TYPE fb_get_virtual_buffer_size( unsigned int* pVWidth, unsigned int* pVHeight )
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00040004;  // Tag: get virtual buffer size 
    pBuffData[off++] = 8;           // response buffer size in bytes
    pBuffData[off++] = 0;           // request size 
    pBuffData[off++] = 0;           // response buffer
    pBuffData[off++] = 0;           // response buffer
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    postman_send( 8, mem_v2p((unsigned int)pBuffData) );
    postman_recv( 8, &respmsg);

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    if( pVWidth )
        *pVWidth = pBuffData[5];
    if( pVHeight )
        *pVHeight = pBuffData[6];

    return FB_SUCCESS;
}


FB_RETURN_TYPE fb_set_virtual_buffer_size( unsigned int* pWidth, unsigned int* pHeight )
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00048004;  // Tag: set virtual buffer size 
    pBuffData[off++] = 8;           // response buffer size in bytes
    pBuffData[off++] = 8;           // request size 
    pBuffData[off++] = *pWidth;       // response buffer
    pBuffData[off++] = *pHeight;      // response buffer
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    postman_send( 8, mem_v2p((unsigned int)pBuffData) );
    postman_recv( 8, &respmsg);

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    if( pWidth )
        *pWidth = pBuffData[5];
    if( pHeight )
        *pHeight = pBuffData[6];

    return FB_SUCCESS;
}


FB_RETURN_TYPE fb_set_virtual_offset( unsigned int pX, unsigned int pY )
{
    volatile unsigned int pBuffData[256] __attribute__((aligned (16)));
    unsigned int off;
    unsigned int respmsg;

    off=1;
    pBuffData[off++] = 0;           // Request 
    pBuffData[off++] = 0x00048009;  // Tag: set virtual offset 
    pBuffData[off++] = 8;           // response buffer size in bytes
    pBuffData[off++] = 8;           // request size 
    pBuffData[off++] = pX;           // response buffer
    pBuffData[off++] = pY;           // response buffer
    pBuffData[off++] = 0;           // end tag

    pBuffData[0]=off*4; // Total message size

    postman_send( 8, mem_v2p((unsigned int)pBuffData) );
    postman_recv( 8, &respmsg);

    if( pBuffData[1]!=0x80000000 )
    {
        return FB_ERROR;
    }

    return FB_SUCCESS;
}
