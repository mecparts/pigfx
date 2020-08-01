#include "pigfx_config.h"
#include "gfx.h"
#include "console.h"
#include "dma.h"
#include "utils.h"
#include "uart.h"
#include "ee_printf.h"
#include "framebuffer.h"
#include <stdio.h>
# define M_PI   3.14159265358979323846  /* pi */
# define M_PI_2 1.57079632679489661923  /* pi/2 */

extern unsigned char G_FONT_GLYPHS08;
extern unsigned char G_FONT_GLYPHS14;
extern unsigned char G_FONT_GLYPHS16;

// default FONT_HEIGHT is configurable in pigfx_config.h but 
// FONT_WIDTH is fixed at 8 for implementation efficiency
// (one character line fits exactly in two 32-bit integers)
#define FONT_WIDTH 8


#define MIN( v1, v2 ) ( ((v1) < (v2)) ? (v1) : (v2))
#define MAX( v1, v2 ) ( ((v1) > (v2)) ? (v1) : (v2))
#define PFB( X, Y ) ( ctx.pfb + (Y)*ctx.Pitch + (X) )

void __swap__( int* a, int* b )
{
    int aux = *a;
    *a = *b;
    *b = aux;
}

int __abs__( int a )
{
    return a<0?-a:a;
}

void b2s(char *b, unsigned char n)
{
  *b++ = '0' + n/100;
  n -= 100*(n/100);
  *b++ = '0' + n/10;
  n -= 10*(n/10);
  *b++ = '0' + n;
}


/* state forward declarations */
typedef void state_fun( char ch, scn_state *state );
void state_fun_alt_charset( char ch, scn_state *state );
void state_fun_normaltext( char ch, scn_state *state );
void state_fun_read_digit( char ch, scn_state *state );
void gfx_restore_cursor_content();


typedef struct {
    unsigned int W;
    unsigned int H;
    unsigned int Pitch;
    unsigned int size;
    unsigned char* pfb;

    unsigned char *full_pfb;
    unsigned int full_size;
    unsigned int full_height;
 
    struct
    {
        unsigned int WIDTH;
        unsigned int HEIGHT;
        unsigned int cursor_row;
        unsigned int cursor_col;
        unsigned int saved_cursor[2];
        char cursor_visible;

        scn_state state;
    } term;

    GFX_COL bg;
    GFX_COL fg;
    unsigned int inverse;
    unsigned int underline;
    unsigned int alt_charset;

    unsigned int   line_limit;

    int            font_height;
    unsigned char* font_data;

    unsigned int cursor_buffer[32*2];

} FRAMEBUFFER_CTX;

GFX_COL default_bg = DEFAULT_BG;
GFX_COL default_fg = DEFAULT_FG;

static FRAMEBUFFER_CTX ctx;
unsigned int __attribute__((aligned(0x100))) mem_buff_dma[16];


#define MKCOL32(c)    ((c)<<24 | (c)<<16 | (c)<<8 | (c))
#define GET_BG32(ctx) (ctx.inverse ? MKCOL32(ctx.fg) : MKCOL32(ctx.bg))
#define GET_FG32(ctx) (ctx.inverse ? MKCOL32(ctx.bg) : MKCOL32(ctx.fg))
#define GET_BG(ctx)   (ctx.inverse ? ctx.fg : ctx.bg)
#define GET_FG(ctx)   (ctx.inverse ? ctx.bg : ctx.fg)

static  gsx_state gsx;

extern char *u2s(unsigned int u);
void gfx_term_render_cursor();


void gfx_toggle_font_height()
{
  if( ctx.font_height == 8 )
    gfx_set_font_height(14);
  else if( ctx.font_height == 14 )
    gfx_set_font_height(16);
  else
    gfx_set_font_height(8);
}


void gfx_set_screen_geometry()
{
  unsigned int lines, border_top_bottom = 0;

  lines = ctx.full_height / ctx.font_height;
  if( ctx.line_limit>0 && ctx.line_limit < lines ) lines = ctx.line_limit;
  border_top_bottom = (ctx.full_height-(lines*ctx.font_height))/2;
  if( ctx.line_limit==0 && border_top_bottom==0 ) border_top_bottom = ctx.font_height/2;

  gfx_clear();

  ctx.pfb  = ctx.full_pfb + (border_top_bottom*ctx.Pitch);
  ctx.H    = ctx.full_height - (border_top_bottom*2);
  ctx.size = ctx.full_size  - (border_top_bottom*2*ctx.Pitch);

  ctx.term.HEIGHT = ctx.H / ctx.font_height;
  ctx.term.cursor_row = ctx.term.cursor_col = 0;

  gfx_term_render_cursor();
}


void gfx_toggle_lines()
{
  if( ctx.line_limit==0 ) ctx.line_limit = ctx.term.HEIGHT;

  ctx.line_limit--;
  if( ctx.line_limit<16 ) ctx.line_limit = ctx.full_height/ctx.font_height;
  
  gfx_set_screen_geometry();
  gfx_term_putstring("["); gfx_term_putstring(u2s(ctx.line_limit)); gfx_term_putstring(" lines]\r\n");
}


void gfx_set_font_height(unsigned int h)
{
  ctx.font_height = h;
  switch( h )
    {
    case  8: ctx.font_data = &G_FONT_GLYPHS08; break;
    case 14: ctx.font_data = &G_FONT_GLYPHS14; break;
    case 16: ctx.font_data = &G_FONT_GLYPHS16; break;
    }

  ctx.line_limit = MIN(ctx.line_limit, ctx.full_height/h);
  gfx_set_screen_geometry();
}


void gfx_set_env( void* p_framebuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int size )
{
    dma_init();

    ctx.full_pfb    = p_framebuffer;
    ctx.full_height = height;
    ctx.full_size   = size;
    ctx.W           = width;
    ctx.Pitch       = pitch;

    ctx.term.WIDTH = ctx.W / FONT_WIDTH;
    ctx.term.cursor_visible = 1;
    ctx.term.state.next = state_fun_normaltext;

    ctx.bg = DEFAULT_BG;
    ctx.fg = DEFAULT_FG;
    ctx.inverse = 0;
    ctx.underline = 0;
    ctx.alt_charset = 0;
    ctx.line_limit = SCREEN_LINES;

    gfx_set_font_height(FONT_HEIGHT);
}


void gfx_term_reset_attrib()
{
  gfx_set_bg(default_bg);
  gfx_set_fg(default_fg);
  ctx.inverse = 0;
  ctx.underline = 0;
}

void gfx_set_bg( GFX_COL col )
{
    ctx.bg = col;
}


void gfx_set_fg( GFX_COL col )
{
    ctx.fg = col;
}


void gfx_swap_fg_bg()
{
    ctx.inverse = !ctx.inverse;
}


void gfx_get_term_size( unsigned int* rows, unsigned int* cols )
{
    *rows = ctx.term.HEIGHT;
    *cols = ctx.term.WIDTH;
}


void gfx_clear()
{
#if ENABLED(GFX_USE_DMA)
    unsigned int* BG = (unsigned int*)mem_2uncached( mem_buff_dma );
    *BG = GET_BG32(ctx);
    *(BG+1) = *BG;
    *(BG+2) = *BG;
    *(BG+3) = *BG;

    dma_enqueue_operation( BG,
            (unsigned int *)( ctx.pfb ),
            ctx.size,
            0,
            DMA_TI_DEST_INC );

    dma_execute_queue();
#else
    unsigned char* pf = ctx.pfb;
    unsigned char* pfb_end = pf + ctx.size;
    while(pf < pfb_end)
      *pf++ = GET_BG(ctx);
#endif
}


void gfx_scroll_down_dma( unsigned int npixels )
{
    unsigned int* BG = (unsigned int*)mem_2uncached( mem_buff_dma );
    *BG = GET_BG32(ctx);
    *(BG+1) = *BG;
    *(BG+2) = *BG;
    *(BG+3) = *BG;
    unsigned int line_height = ctx.Pitch * npixels;


    dma_enqueue_operation( (unsigned int *)( ctx.pfb + line_height ),
                           (unsigned int *)( ctx.pfb ),
                           (ctx.size - line_height),
                           0,
                           DMA_TI_SRC_INC | DMA_TI_DEST_INC );

    dma_enqueue_operation( BG,
                           (unsigned int *)( ctx.pfb + ctx.size -line_height ),
                           line_height,
                           0,
                           DMA_TI_DEST_INC );
}


void gfx_scroll_down( unsigned int npixels )
{
#if ENABLED(GFX_USE_DMA)
    gfx_scroll_down_dma( npixels );
    dma_execute_queue();
#else
    unsigned int* pf_src = (unsigned int*)( ctx.pfb + ctx.Pitch*npixels);
    unsigned int* pf_dst = (unsigned int*)ctx.pfb;
    const unsigned int* const pfb_end = (unsigned int*)( ctx.pfb + ctx.size );

    while( pf_src < pfb_end )
        *pf_dst++ = *pf_src++;

    // Fill with bg at the bottom
    const unsigned int BG = GET_BG32(ctx);
    while( pf_dst < pfb_end )
        *pf_dst++ = BG;

#endif
}


void gfx_scroll_up( unsigned int npixels )
{
    unsigned int* pf_dst = (unsigned int*)( ctx.pfb + ctx.size ) -1;
    unsigned int* pf_src = (unsigned int*)( ctx.pfb + ctx.size - ctx.Pitch*npixels) -1;
    const unsigned int* const pfb_end = (unsigned int*)( ctx.pfb );

    while( pf_src >= pfb_end )
        *pf_dst-- = *pf_src--;

    // Fill with bg at the top
    const unsigned int BG = GET_BG32(ctx);

    while( pf_dst >= pfb_end )
        *pf_dst-- = BG;
}


void gfx_delete_lines( unsigned int row, unsigned int n )
{
    unsigned int* pf_dst = (unsigned int*)( ctx.pfb + ctx.Pitch*ctx.font_height*row);
    unsigned int* pf_src = (unsigned int*)( ctx.pfb + ctx.Pitch*ctx.font_height*(row+n));
    const unsigned int* const pfb_end = (unsigned int*)( ctx.pfb + ctx.size );

    while( pf_src < pfb_end )
        *pf_dst++ = *pf_src++;

    // Fill with bg at the bottom
    const unsigned int BG = GET_BG32(ctx);
    while( pf_dst < pfb_end )
        *pf_dst++ = BG;
}


void gfx_insert_lines( unsigned int row, unsigned int n )
{
    unsigned int* pf_dst = (unsigned int*)( ctx.pfb + ctx.size ) -1;
    unsigned int* pf_src = (unsigned int*)( ctx.pfb + ctx.size - ctx.Pitch*ctx.font_height*n) -1;
    const unsigned int* const pfb_end = (unsigned int*)( ctx.pfb + ctx.Pitch*ctx.font_height*row);

    while( pf_src >= pfb_end )
        *pf_dst-- = *pf_src--;

    // Fill with bg at the row
    const unsigned int BG = GET_BG32(ctx);

    ++pf_src;
    while( pf_src <= pf_dst )
        *pf_src++ = BG;
}


void gfx_fill_rect_dma( unsigned int x, unsigned int y, unsigned int width, unsigned int height )
{
    unsigned int* FG = (unsigned int*)mem_2uncached( mem_buff_dma )+4;
    *FG = GET_FG32(ctx);
    *(FG+1) = *FG;
    *(FG+2) = *FG;
    *(FG+3) = *FG;

    dma_enqueue_operation( FG,
                           (unsigned int *)( PFB(x,y) ),
                           (((height-1) & 0xFFFF )<<16) | (width & 0xFFFF ),
                           ((ctx.Pitch-width) & 0xFFFF)<<16, /* bits 31:16 destination stride, 15:0 source stride */
                           DMA_TI_DEST_INC | DMA_TI_2DMODE );
}


void gfx_fill_rect( unsigned int x, unsigned int y, unsigned int width, unsigned int height )
{
  if( x >= ctx.W || y >= ctx.H )
    return;

  if( x+width > ctx.W )
    width = ctx.W-x;

  if( y+height > ctx.H )
    height = ctx.H-y;

#if ENABLED(GFX_USE_DMA)
    gfx_fill_rect_dma( x, y, width, height );
    dma_execute_queue();
#else
    while( height-- )
    {
        unsigned char* pf = PFB(x,y);
        const unsigned char* const pfb_end = pf + width;

        while( pf < pfb_end )
          *pf++ = GET_FG(ctx);

        ++y;
    }
#endif
}


void gfx_clear_rect( unsigned int x, unsigned int y, unsigned int width, unsigned int height )
{
    GFX_COL curr_fg = ctx.fg;
    ctx.fg = ctx.bg;
    gfx_fill_rect(x, y, width, height);
    ctx.fg = curr_fg;
}


void gfx_line( int x0, int y0, int x1, int y1, GFX_COL clr )
{
    x0 = MAX( MIN(x0, (int)ctx.W - 1), 0 );
    y0 = MAX( MIN(y0, (int)ctx.H - 1), 0 );
    x1 = MAX( MIN(x1, (int)ctx.W - 1), 0 );
    y1 = MAX( MIN(y1, (int)ctx.H - 1), 0 );

    unsigned char qrdt = __abs__(y1 - y0) > __abs__(x1 - x0);

    if (qrdt) {
        __swap__(&x0, &y0);
        __swap__(&x1, &y1);
    }
    if (x0 > x1) {
        __swap__(&x0, &x1);
        __swap__(&y0, &y1);
    }

    const int deltax = x1 - x0;
    const int deltay = __abs__(y1 - y0);
    register int error = deltax >> 1;
    register unsigned char* pfb;
    unsigned int nr = x1 - x0 + 1;

    if( qrdt ) {
        const int ystep = y0 < y1 ? 1 : -1;
        pfb = PFB(y0, x0);
        while (nr--) {
            *pfb = clr;
            error = error - deltay;
            if (error < 0) {
                pfb += ystep;
                error += deltax;
            }
            pfb += ctx.Pitch;
        }
    } else {
        const int ystep = y0 < y1 ? ctx.Pitch : -ctx.Pitch;
        pfb = PFB(x0, y0);
        while (nr--) {
            *pfb = clr;
            error = error - deltay;
            if (error < 0) {
                pfb += ystep;
                error += deltax;
            }
            pfb++;
        }
    }
}

void gfx_pixel( int x, int y, GFX_COL clr ) {
    if (x >= 0 && x < (int)ctx.W && y >= 0 && y < (int)ctx.H) {
        register unsigned char* pfb;
        pfb = PFB(x, y);
        *pfb = clr;
    }
}

//
// A utility routine used by filled_polygon and 
// drawing_primitive to draw a horizontal line between
// (x0,y) and (x1,y) using the current fill parameters
// (colour, style and style index).
//
void gfx_filled_hline(int x0, int x1, int y, GFX_COL clr) {

    switch (gsx.fill_style) {
        
        case FS_HOLLOW:
            break;
            
        case FS_SOLID:
            gfx_line(x0, y, x1, y, clr);
            break;
            
        case FS_HALFTONE:
            switch (gsx.fill_index) {
                
                case 1:    // hollow
                    break;
                    
                case 2:    // very dark
                    for (int x = x0; x <= x1; ++x) {
                        if (halftone1[y & 0x1F] & 1 << (x & 0x1F)) {
                            gfx_pixel(x, y, clr);
                        }
                    }
                    break;
                    
                case 3:    // dark
                    for (int x = x0; x <= x1; ++x) {
                        if (halftone2[y & 0x1F] & 1 << (x & 0x1F)) {
                            gfx_pixel(x, y, clr);
                        }
                    }
                    break;
                    
                case 4:    // light
                    for (int x = x0; x <= x1; ++x) {
                        if (halftone3[y & 0x1F] & 1 << (x & 0x1F)) {
                            gfx_pixel(x, y, clr);
                        }
                    }
                    break;
                    
                case 5:    // very light
                    for (int x = x0; x <= x1; ++x) {
                        if (halftone4[y & 0x1F] & 1 << (x & 0x1F)) {
                            gfx_pixel(x, y, clr);
                        }
                    }
                    break;
                    
                case 6:    // solid
                    gfx_line(x0, y, x1, y, clr);
                    break;
            }
            break;
            
        case FS_HATCH:
            switch (gsx.fill_index) {
                
                case FSI_VERTICAL:
                    for (int x = x0; x <= x1; ++x) {
                        if( !(x & 3) ) {
                            gfx_pixel(x, y, clr);
                        }
                    }
                    break;
                    
                case FSI_HORIZONTAL:
                    if( !(y & 3) ) {
                        gfx_line(x0, y, x1, y, clr);
                    }
                    break;
                    
                case FSI_PLUS45:
                    for (int x = x0; x <= x1; ++x) {
                        if (!((x+ y) % 5)) {
                            gfx_pixel(x, y, clr);
                        }
                    }
                    break;
                    
                case FSI_MINUS45:
                    for (int x = x0; x <= x1; ++x) {
                        if (!((x - y) % 5)) {
                            gfx_pixel(x, y, clr);
                        }
                    }
                    break;
                    
                case FSI_CROSS:
                    if( !(y & 3) ) {
                        gfx_line(x0, y, x1, y, clr);
                    } else {
                        for (int x = x0; x <= x1; ++x) {
                            if( !(x & 3) ) {
                                gfx_pixel(x, y, clr);
                            }
                        }
                    }
                    break;
                    
                case FSI_X:
                    for (int x =x0; x <= x1; ++x) {
                        if (!((x + y) % 5) || !((x - y) % 5)) {
                            gfx_pixel(x, y, clr);
                        }
                    }
                    break;
            }
            break;
    }
}

// GSX 1: open workstation
void gfx_open_workstation(scn_state *state) {
    gfx_close_workstation();
    gsx.device_number     = state->cmd_params[1];
    gsx.line_style        = state->cmd_params[2];
    gsx.line_colour       = state->cmd_params[3];
    gsx.marker_style      = state->cmd_params[4];
    gsx.marker_colour     = state->cmd_params[5];
    gsx.text_style        = state->cmd_params[6];
    gsx.text_colour       = state->cmd_params[7];
    gsx.fill_style        = state->cmd_params[8]; 
    gsx.fill_index        = state->cmd_params[9]; 
    gsx.fill_colour       = state->cmd_params[10]; 
    gsx.text_height       = TH_MIN;
    gsx.text_width        = TW_MIN;
    gsx.text_size         = 1;
    gsx.text_direction    = 0;
    gsx.text_font         = 1;
    gsx.line_width        = 1;
    gsx.marker_height     = 1;        // minimum marker height
    gsx.writing_mode      = WM_REPLACE;
    for (int i = 0; i < LID_MAX; ++i ) {
        gsx.input_mode[i] = IM_REQUEST;
    }
    gsx.reverse_video     = 0;
    gfx_term_set_cursor_visibility(0);
    gfx_restore_cursor_content();
    gfx_clear_workstation();
}

// GSX 2: close workstation REQ for CRT
void gfx_close_workstation() {
    gfx_term_set_cursor_visibility(1);
    gfx_restore_cursor_content();
}

// GSX 3: clear workstation REQ for CRT
void gfx_clear_workstation() {
    gfx_term_move_cursor(0, 0);
    gfx_term_clear_screen();
}

// GSX 4: update workstation REQ for CRT
void gfx_update_workstation() {
}

// GSX 5:  escape REQ for CRT
void gfx_escape(scn_state *state) {
    switch (state->cmd_params[1]) {
        
        case 2:    // enter graphics mode REQ for CRT
            gfx_term_set_cursor_visibility(0);
            gfx_term_render_cursor();
            break;
            
        case 3:    // exit graphics mode REQ for CRT
            gfx_term_set_cursor_visibility(1);
            gfx_term_render_cursor();
            break;
            
        case 4:    // cursor up REQ for CRT
            gfx_term_move_cursor(MAX(0,(int) ctx.term.cursor_row-1), ctx.term.cursor_col);
            break;
            
        case 5:    // cursor down REQ for CRT
            gfx_term_move_cursor(MIN((int) ctx.term.HEIGHT-1, (int) ctx.term.cursor_row+1), ctx.term.cursor_col);
            break;
            
        case 6:    // cursor right REQ for CRT
            gfx_term_move_cursor(ctx.term.cursor_row, MIN((int) ctx.term.WIDTH-1, (int) ctx.term.cursor_col+1));
            break;
            
        case 7:    // cursor left REQ for CRT
            gfx_term_move_cursor(ctx.term.cursor_row, MAX(0, (int) ctx.term.cursor_col-1));
            break;
            
        case 8:    // home cursor REQ for CRT
            gfx_term_move_cursor(0, 0);
            break;
            
        case 9:    // erase to end of screen REQ for CRT
            gfx_term_clear_till_end();
            if( ctx.term.cursor_row+1 < ctx.term.HEIGHT ) {
                gfx_term_clear_lines(ctx.term.cursor_row+1, ctx.term.HEIGHT-1);
            }
            break;
            
        case 10:    // erase to end of line REQ for CRT
            gfx_term_clear_till_end();
            break;
            
        case 11: // direct cursor address REQ for CRT
            {
                unsigned int r = state->cmd_params[2];
                unsigned int c = state->cmd_params[3];
                gfx_term_move_cursor(MIN(r-1, ctx.term.HEIGHT-1), MIN(c-1, ctx.term.WIDTH-1));
            }
            break;
            
        case 12:    // direct cursor addressable text REQ for CRT
            for (unsigned int i = 0; i < state->cmd_params[2]; ++i) {
               gfx_putc( ctx.term.cursor_row, ctx.term.cursor_col, state->cmd_params[3 + i] );
               ++ctx.term.cursor_col;
               gfx_term_render_cursor();
            }
            break;
            
        case 13:    // reverse video on
            ctx.inverse = 1;                // reverse on
            break;
            
        case 14:    // reverse video off
            ctx.inverse = 0;                // reverse on
            break;
            
        case 15:    // inquire current current cursor address REQ for CRT
            // TODO
            break;
            
        case 16:    // inquire tablet status
            // TODO
            break;
            
        case 17:    // hardcopy
            // TODO
            break;
            
        case 18:    // place graphic cursor at location REQ for CRT
            gfx_place_graphic_cursor(state->cmd_params[2], state->cmd_params[3]);
            break;
            
        case 19:    // remove last graphic cursor REQ for CRT
            gfx_remove_graphic_cursor();
            break;
    }
}

void gfx_place_graphic_cursor(int x, int y) {
   gsx.gc_ulx = x - GSX_GC_ARMLNG;
   gsx.gc_uly = y - GSX_GC_ARMLNG;
   gsx.gc_w = GSX_GC_W;
   gsx.gc_h = GSX_GC_W;
   if (gsx.gc_ulx < 0) {
        gsx.gc_w += gsx.gc_ulx;
    gsx.gc_ulx = 0;
    }
    if (gsx.gc_ulx + gsx.gc_w > GSX_MAX_NDC) {
        gsx.gc_w = GSX_MAX_NDC - gsx.gc_ulx + 1;
    }
    if (gsx.gc_uly < 0) {
        gsx.gc_h += gsx.gc_uly;
        gsx.gc_uly = 0;
    }
    if (gsx.gc_uly + gsx.gc_h > GSX_MAX_NDC) {
        gsx.gc_h = GSX_MAX_NDC - gsx.gc_uly + 1;
    }
    gfx_read_rect(gsx.gc_ulx, gsx.gc_uly, gsx.gc_w, gsx.gc_h, gsx.gc_rect);
    
    register unsigned char* pfb;
    for (int i = gsx.gc_ulx; i < gsx.gc_ulx + gsx.gc_w; ++i) {
        pfb = PFB(i, y);
        *pfb = get_xored_index(*pfb);
    }
    for (int i = gsx.gc_uly; i < gsx.gc_uly + gsx.gc_h; ++i) {
        pfb = PFB(x, i);
        *pfb = get_xored_index(*pfb);
    }
}

void gfx_remove_graphic_cursor(void) {
    gfx_write_rect(gsx.gc_ulx, gsx.gc_uly, gsx.gc_w, gsx.gc_h, gsx.gc_rect);
}

void gfx_read_rect(int ulx, int uly, int w, int h,unsigned char* rect) {
    register unsigned char* pfb;
    for (int y = 0; y < h; ++y) {
        pfb = PFB(ulx, (uly + y));
        for (int x = 0; x < w; ++x) {
            *rect++ = *pfb++;
        }
    }
}

void gfx_write_rect(int ulx, int uly, int w, int h,unsigned char* rect) {
    register unsigned char* pfb;
    for (int y = 0; y < h; ++y) {
        pfb = PFB(ulx, (uly + y));
        for (int x = 0; x < w; ++x) {
            *pfb++ = *rect++;
        }
    }
}

// GSX 6: draw polyline REQ for CRT
void gfx_draw_polyline(scn_state *state) {
    int nPoints = state->cmd_params[1] - 1;
    int x0, y0, x1, y1;
    
    for (int i = 0; i < nPoints * 2; i += 2) {
        x0 = state->cmd_params[i + 2];
        y0 = state->cmd_params[i + 3];
        x1 = state->cmd_params[i + 4];
        y1 = state->cmd_params[i + 5];
        switch (gsx.line_style) {
            case LS_DASH:       // 1111111100000000
            case LS_DOT:        // 1110000011100000
            case LS_DASH_DOT:   // 1111111000111000
            case LS_LONG_DASH:  // 1111111111110000
            case LS_SOLID:
            default:
                // TODO: line style
                gfx_line(x0, y0, x1, y1, gsx.line_colour);
                break;
        }
    }
}

// GSX 7: plot a group of markers REQ for CRT
void gfx_draw_polymarkers(scn_state *state) {
    int nMarkers = state->cmd_params[1];
    unsigned int armLng = (gsx.marker_height-1)/2;
    unsigned int shortArmLng = (unsigned int)(armLng * 0.707);
    
    for (int i = 0; i < nMarkers; ++i) {
        int x = state->cmd_params[i + i + 2];
        int y = state->cmd_params[i + i + 3];
        
        switch (gsx.marker_style) {
            
            case MS_DOT:
                gfx_pixel(x, y, gsx.marker_colour);
                break;
                
            case MS_PLUS:
                gfx_line(x - armLng, y, x + armLng, y, gsx.marker_colour);
                gfx_line(x, y - armLng, x, y + armLng, gsx.marker_colour);
                break;
                
            case MS_ASTERISK:
                gfx_line(x - armLng, y, x + armLng, y, gsx.marker_colour);
                gfx_line(x, y - armLng, x, y + armLng, gsx.marker_colour);
                gfx_line(x - shortArmLng, y - shortArmLng, x + shortArmLng, y + shortArmLng, gsx.marker_colour);
                gfx_line(x - shortArmLng, y + shortArmLng, x + shortArmLng, y - shortArmLng, gsx.marker_colour);
                break;
                
            case MS_CIRCLE:
                gfx_circle(x, y, armLng, gsx.marker_colour, 0);
                break;
                
            case MS_CROSS:
                gfx_line(x - armLng, y - armLng, x + armLng, y + armLng, gsx.marker_colour);
                gfx_line(x - armLng, y + armLng, x + armLng, y - armLng, gsx.marker_colour);
                break;
        }
    }
}

// GSX 8: draw text REQ for CRT
void gfx_draw_text(scn_state *state) {
    unsigned int nChars = state->cmd_params[1];
    int x = state->cmd_params[2];
    int y = state->cmd_params[3];
    register unsigned char* p_glyph;
    char c;

    // Shift character so that x,y is the baseline, not the top
    switch (gsx.text_direction) {
        case 900:
            x -= gsx.text_height - 1;
            break;
        case 1800:
            y += gsx.text_height - 1;
            break;
        case 2700:
            x += gsx.text_height - 1;
            break;
        case 0:
        default:
            y -= gsx.text_height - 1;
            break;
    }
    for (unsigned int i = 0; i < nChars; ++i ) {
        c = (char)state->cmd_params[i + 4];
        p_glyph = (unsigned char*)( ctx.font_data + ((unsigned int)c*FONT_WIDTH*ctx.font_height) );
        for (int yy = 0; yy < ctx.font_height; ++yy ) {
            for (int xx = 0; xx < FONT_WIDTH; ++xx ) {
                if (*p_glyph++) {
                    switch (gsx.text_direction) {
                        case 900:
                            if (gsx.text_size == 1) {
                                gfx_pixel( x + yy, y - xx, gsx.text_colour );
                            } else {
                                for (int i = 0; i < gsx.text_size; ++i ) {
                                    for (int j = 0; j < gsx.text_size; ++j ) {
                                        gfx_pixel( x + yy * gsx.text_size + i, y - xx * gsx.text_size - j, gsx.text_colour );
                                    }
                                }
                            }
                            break;
                        case 1800:
                            if (gsx.text_size == 1) {
                                gfx_pixel( x - xx, y - yy, gsx.text_colour );
                            } else {
                                for (int i = 0; i < gsx.text_size; ++i ) {
                                    for (int j = 0; j < gsx.text_size; ++j ) {
                                        gfx_pixel( x - xx * gsx.text_size - i, y - yy * gsx.text_size - j, gsx.text_colour );
                                    }
                                }
                            }
                            break;
                        case 2700:
                            if (gsx.text_size == 1) {
                                gfx_pixel( x - yy, y + xx, gsx.text_colour );
                            } else {
                                for (int i = 0; i < gsx.text_size; ++i ) {
                                    for (int j = 0; j < gsx.text_size; ++j ) {
                                        gfx_pixel( x - yy * gsx.text_size - i, y + xx * gsx.text_size + j, gsx.text_colour );
                                    }
                                }
                            }
                            break;
                        case 0:
                        default:
                            if (gsx.text_size == 1) {
                                gfx_pixel( x + xx, y + yy, gsx.text_colour );
                            } else {
                                for (int i = 0; i < gsx.text_size; ++i ) {
                                    for (int j = 0; j < gsx.text_size; ++j ) {
                                        gfx_pixel( x + xx * gsx.text_size + i, y + yy * gsx.text_size + j, gsx.text_colour );
                                    }
                                }
                            }
                            break;
                    }
                }
            }
        }
        switch (gsx.text_direction) {
            case 900:
                y -= FONT_WIDTH * gsx.text_size;
                break;
            case 1800:
                x -= FONT_WIDTH * gsx.text_size;
                break;
            case 2700:
                y += FONT_WIDTH * gsx.text_size;
                break;
            case 0:
            default:
                x += FONT_WIDTH * gsx.text_size;
                break;
       }
    }
}

// GSX 9: draw a filled polygon REQ for CRT
void gfx_filled_polygon(scn_state *state) {

    int nPoints = state->cmd_params[1];
    int dy,dx;
    int temp,k;
    int xi[1024];
    float slope[MAX_PTSIN];
    int ymin=GSX_MAX_NDC,ymax=0;
    int xin[MAX_PTSIN + 1],yin[MAX_PTSIN +1];

    for (int i = 0; i< nPoints; ++i) {
        xin[i] = state->cmd_params[2 * i + 2];
        yin[i] = state->cmd_params[2 * i + 3];
    }
    xin[nPoints] = xin[0];
    yin[nPoints] = yin[0];
    
    for (int i = 0; i < nPoints; ++i) {
        int x0 = xin[i];
        int y0 = yin[i];
        int x1 = xin[i + 1];
        int y1 = yin[i + 1];
        gfx_line(x0, y0, x1, y1, gsx.fill_colour);

        if( y0 < ymin ) {
            ymin = y0;
        }
        if( y0 > ymax ) {
            ymax = y0;
        }
    }
    
    if (gsx.fill_style != FS_HOLLOW) {

        for (int i = 0 ; i < nPoints ; ++i) {
            dy = yin[i + 1] - yin[i];    // dy=y2-y1
            dx = xin[i + 1] - xin[i];    // dx=x2-x1

            if (dy == 0) {
                slope[i] = 1.0;
            }
            if (dx == 0) {
                slope[i] = 0.0;
            }
            if (dy != 0 && dx != 0) {            // calculate inverse slope
                slope[i] = (float)dx / dy;        // typecast to float
            }
        }

        for (int y = ymin; y <= ymax; ++y) {        // maximum range of y length resolution is 480 
            k = 0;
            for (int i = 0; i < nPoints; ++i) {
                int x0 = xin[i];
                int y0 = yin[i];
                int y1 = yin[i + 1];
                if ((y0 <= y && y1 > y) || (y0 > y && y1 <= y)) {
                    xi[k] = (int)(x0 + slope[i] * (y - y0));
                    ++k;
                }
            }

            for (int i = 0; i < k - 1; ++i) {        // Arrange x-intersections in order
                for (int j = 0; j < k - 1; ++j) {
                    if (xi[j] > xi[j + 1]) {    // sort x intersection in xi[k] in order using swapping
                      temp = xi[j];
                      xi[j] = xi[j + 1];
                      xi[j + 1] = temp;
                    }
                }
            }
            for (int i = 0; i < k; i += 2) {        // draw lines to fill polygon
                gfx_filled_hline(xi[i], xi[i + 1], y, gsx.fill_colour);
            }
        }
    }
}

// Draw a circle with the current fill characteristic.
// Basic code pinched from Adafruit GFX library.
// (Bresenham circle algorithm)
void gfx_circle(int x0, int y0, int r, GFX_COL clr, boolean filled) {
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    gfx_pixel(x0, y0 + r, clr);
    gfx_pixel(x0, y0 - r, clr);
    gfx_pixel(x0 + r, y0, clr);
    gfx_pixel(x0 - r, y0, clr);
    if (filled) {
        gfx_filled_hline(x0 - r, x0 + r, y0, clr);
    }
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        gfx_pixel(x0 + x, y0 + y, clr);
        gfx_pixel(x0 - x, y0 + y, clr);
        gfx_pixel(x0 + x, y0 - y, clr);
        gfx_pixel(x0 - x, y0 - y, clr);
        gfx_pixel(x0 + y, y0 + x, clr);
        gfx_pixel(x0 - y, y0 + x, clr);
        gfx_pixel(x0 + y, y0 - x, clr);
        gfx_pixel(x0 - y, y0 - x, clr);
        if (filled) {
            gfx_filled_hline(x0 - x, x0 + x, y0 + y, clr);
            gfx_filled_hline(x0 - x, x0 + x, y0 - y, clr);
            gfx_filled_hline(x0 - y, x0 + y, y0 + x, clr);
            gfx_filled_hline(x0 - y, x0 + y, y0 - x, clr);
        }
    }
}

// approximate atan2 function for bare metal w/o math library
float atan(float z) {
    const float n1 = 0.97239411f;
    const float n2 = -0.19194795f;
    return (n1 + n2 * z * z) * z;
}

float atan2(int y, int x) {
    if (x) {
        int absx = x, absy = y;
        if (absx < 0) {
            absx = -absx;
        }
        if (absy < 0) {
            absy = -absy;
        }
        if (absx > absy) {
            const float z = y / (1.0f * x);
            if (x > 0) {
                return atan(z);
            } else if (y >= 0) {
                return atan(z) + M_PI;
            } else {
                return atan(z) - M_PI;
            }
        } else {
            const float z = x / (1.0f * y);
            if (y > 0) {
                return -atan(z) + M_PI_2;
            } else {
                return -atan(z) - M_PI_2;
            }
        }
    } else {
        if (y > 0) {
            return M_PI_2;
        } else if (y < 0) {
            return -M_PI_2;
        }
    }
    return 0.0f;
}

//
// 0 degrees is defined as 90 degrees clockwise from vertical.
// degrees increase in the counter clockwise direction.
//
void gfx_draw_arc_point(int x0, int y0, int x, int y, int a0, int a1, GFX_COL clr) {
   if (x <= y) {
      // if both x and y are >= 0, returned angle will be in [45.. 90] 
      int angle = 10 * (int)(0.5f + 180 * atan2(y, x) / M_PI);
      
      if (900 - angle >= a0 && 900 - angle <= a1) {   //   0 -  45
         gfx_pixel(x0 + y, y0 - x, clr);
        }
      if (angle >= a0 && angle <= a1) {               //  45 -  90
         gfx_pixel(x0 + x, y0 - y, clr);
        }
      if (1800 - angle >= a0 && 1800 - angle <= a1) { //  90 - 135
         gfx_pixel(x0 - x, y0 - y, clr);
        }
      if (angle + 900 >= a0 && angle + 900 <= a1) {   // 135 - 180
         gfx_pixel(x0 - y, y0 - x, clr);
        }
      if (2700 - angle >= a0 && 2700 - angle <= a1) { // 180 - 225
         gfx_pixel(x0 - y, y0 + x, clr);
        }
      if (angle + 1800 >= a0 && angle + 1800 <= a1) { // 225 - 270
         gfx_pixel(x0 - x, y0 + y, clr);
        }
      if (3600 - angle >= a0 && 3600 - angle <= a1) { // 270 - 315
         gfx_pixel(x0 + x, y0 + y, clr);
        }
      if (angle + 2700 >= a0 && angle + 2700 <= a1) { // 315 - 360
         gfx_pixel(x0 + y, y0 + x, clr);
        }
    }
}
         
void gfx_draw_arc(int x0, int y0, int r, int arc_s, int arc_e, GFX_COL clr) {
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    gfx_draw_arc_point(x0, y0, x, y, arc_s, arc_e, clr);
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        gfx_draw_arc_point(x0, y0, x, y, arc_s, arc_e, clr);
    }
}

// GSX 10: draw bitmap REQ for CRT
void gfx_draw_bitmap(scn_state *state) {
    int xpos = state->cmd_params[1];
    int ypos = state->cmd_params[2];
    int w = state->cmd_params[3];
    for (int x = 0; x < w; ++x) {
        gfx_pixel( xpos + x, ypos, state->cmd_params[4 + x]);
    }
}

// GSX 11: general drawing primitive REQ for CRT
void gfx_drawing_primitive(scn_state *state) {
    switch (state->cmd_params[1]) {
        case 1:    // bar REQ for CRT
            {
                int llx = state->cmd_params[2];
                int lly = state->cmd_params[3];
                int urx = state->cmd_params[4];
                int ury = state->cmd_params[5];
                gfx_line(llx, lly, llx, ury, gsx.fill_colour);
                gfx_line(llx, ury, urx, ury, gsx.fill_colour);
                gfx_line(urx, ury, urx, lly, gsx.fill_colour);
                gfx_line(urx, lly, llx, lly, gsx.fill_colour);
                for (int y = ury + 1; y < lly; ++y) {
                    gfx_filled_hline(llx, urx, y, gsx.fill_colour);
                }
            }
            break;
        case 2:    // arc: drawn counter clockwise from arc_s to arc_e
            {
                int x0 = state->cmd_params[2];
                int y0 = state->cmd_params[3];
                int r = state->cmd_params[4];
                int arc_s = state->cmd_params[5];
                int arc_e = state->cmd_params[6];

                if (arc_s <= arc_e) {
                    gfx_draw_arc(x0, y0, r, arc_s, arc_e, gsx.line_colour);
                } else {
                    gfx_draw_arc(x0, y0, r, arc_s, 3600, gsx.line_colour);
                    gfx_draw_arc(x0, y0, r, 0, arc_e, gsx.line_colour);
                }
            }
            break;
        case 3:    // pie slice
            // TODO
            break;
        case 4:    // circle
            gfx_circle(
                state->cmd_params[2],   // x
                state->cmd_params[3],   // y
                state->cmd_params[4],   // r
                gsx.fill_colour,        // clr
                1);                     // filled
            break;
        case 5:    // print graphic characters (ruling characters)
            // TODO
            break;
    }
}

// GSX 12: set text size REQ for CRT
void gfx_set_text_height(scn_state *state) {
    gsx.text_size = state->cmd_params[1];
    gsx.text_height = state->cmd_params[2];
    gsx.text_width = state->cmd_params[3];
}

// GSX 13: set text direction
void gfx_set_text_direction(scn_state *state) {
    gsx.text_direction = state->cmd_params[1];
}

// GSX 14: set colour index (palette registers) REQ for CRT
void gfx_set_palette_colour(scn_state *state) {
    ++state;    // TODO
}

// GSX 27: read bitmap
void gfx_get_bitmap(scn_state *state) {
    ++state;    // TODO
}

// GSX 28: read locator (eg tablet or mouse)
void gfx_input_locator(scn_state *state) {
    ++state;    // TODO
}

// GSX 29: read valuator
void gfx_input_valuator(scn_state *state) {
    ++state;    // TODO
}

// GSX 30: read choice
void gfx_input_choice(scn_state *state) {
    ++state;    // TODO
}

// GSX 31: read string
void gfx_input_string(scn_state *state) {
    ++state;    // TODO
}

// GSX 33: set input mode
void gsx_set_input_mode(scn_state *state) {
    ++state;    // TODO
}

void gfx_putc( unsigned int row, unsigned int col, unsigned char c )
{
    if( col >= ctx.term.WIDTH )
        return;

    if( row >= ctx.term.HEIGHT )
        return;

    const unsigned int FG = GET_FG32(ctx);
    const unsigned int BG = GET_BG32(ctx);
    const unsigned int stride = (ctx.Pitch>>2) - 2;

    register unsigned int* p_glyph = (unsigned int*)( ctx.font_data + ((unsigned int)c*FONT_WIDTH*ctx.font_height) );
    register unsigned int* pf = (unsigned int*)PFB((col*FONT_WIDTH), (row*ctx.font_height));
    register unsigned char h=ctx.font_height;

    while(h--)
    {
        {
            register unsigned int gv = *p_glyph++;
            if( ctx.underline && h==2) {
                gv = ctx.inverse ? BG : FG;
            }
            *pf++ =  (gv & FG) | ( ~gv & BG );
            gv = *p_glyph++;
            if( ctx.underline && h==2 ) {
                gv = ctx.inverse ? BG : FG;
            } 
            *pf++ =  (gv & FG) | ( ~gv & BG );
        }
        pf += stride;
    }
}



/*
 * Terminal functions
 *
 */

void gfx_restore_cursor_content()
{
    // Restore framebuffer content that was overwritten by the cursor
    unsigned int* pb = (unsigned int*)ctx.cursor_buffer;
    unsigned int* pfb = (unsigned int*)PFB( ctx.term.cursor_col*FONT_WIDTH, ctx.term.cursor_row*ctx.font_height );
    const unsigned int stride = (ctx.Pitch>>2) - 2;
    unsigned int h=ctx.font_height;
    while(h--)
    {
        *pfb++ = *pb++;
        *pfb++ = *pb++;
        pfb+=stride;
    }
    //cout("cursor restored");cout_d(ctx.term.cursor_row);cout("-");cout_d(ctx.term.cursor_col);cout_endl();
}


void gfx_term_render_cursor()
{
    // Save framebuffer content that is going to be replaced by the cursor and update
    // the new content
    //
    while( DMA_CHAN0_BUSY ); // Busy wait for DMA

    unsigned char *p;
    unsigned int* pb = (unsigned int*)ctx.cursor_buffer;
    unsigned int* pfb = (unsigned int*)PFB( ctx.term.cursor_col*FONT_WIDTH, ctx.term.cursor_row*ctx.font_height );
    const unsigned int stride = (ctx.Pitch>>2)-2;
    unsigned int h=ctx.font_height;

    if( ctx.term.cursor_visible )
      while(h--)
        {
          //*pb++ = *pfb; *pfb = ~*pfb; pfb++;
          //*pb++ = *pfb; *pfb = ~*pfb; pfb++;
          *pb++ = *pfb;
          p = (unsigned char *) pfb;
          p[0] = (p[0]==ctx.fg) ? ctx.bg : ((p[0]==ctx.bg) ? ctx.fg : ~p[0]);
          p[1] = (p[1]==ctx.fg) ? ctx.bg : ((p[1]==ctx.bg) ? ctx.fg : ~p[1]);
          p[2] = (p[2]==ctx.fg) ? ctx.bg : ((p[2]==ctx.bg) ? ctx.fg : ~p[2]);
          p[3] = (p[3]==ctx.fg) ? ctx.bg : ((p[3]==ctx.bg) ? ctx.fg : ~p[3]);
          pfb++;

          *pb++ = *pfb;
          p = (unsigned char *) pfb;
          p[0] = (p[0]==ctx.fg) ? ctx.bg : ((p[0]==ctx.bg) ? ctx.fg : ~p[0]);
          p[1] = (p[1]==ctx.fg) ? ctx.bg : ((p[1]==ctx.bg) ? ctx.fg : ~p[1]);
          p[2] = (p[2]==ctx.fg) ? ctx.bg : ((p[2]==ctx.bg) ? ctx.fg : ~p[2]);
          p[3] = (p[3]==ctx.fg) ? ctx.bg : ((p[3]==ctx.bg) ? ctx.fg : ~p[3]);
          pfb++;

          pfb+=stride;
        }
    else
      while(h--)
        {
          *pb++ = *pfb++;
          *pb++ = *pfb++;
          pfb+=stride;
        }
}


void gfx_term_render_cursor_newline_dma()
{
    // Fill cursor buffer with the current background and framebuffer with fg
    unsigned int BG = GET_BG32(ctx);

    unsigned int nwords = ctx.font_height*2;
    unsigned int* pb = (unsigned int*)ctx.cursor_buffer;
    while( nwords-- )
    {
        *pb++ = BG;
    }

    if( ctx.term.cursor_visible )
        gfx_fill_rect_dma( ctx.term.cursor_col*FONT_WIDTH, ctx.term.cursor_row*ctx.font_height, FONT_WIDTH, ctx.font_height );
}


void gfx_term_putstring( const char* str )
{
    while( *str )
    {
        while( DMA_CHAN0_BUSY ); // Busy wait for DMA

        switch( *str )
        {
            case '\r':
                gfx_restore_cursor_content();
                ctx.term.cursor_col = 0;
                gfx_term_render_cursor();
                break;

            case '\n':
                gfx_restore_cursor_content();
                ++ctx.term.cursor_row;
                gfx_term_render_cursor();
                break;

            case 0x09: /* tab */
                gfx_restore_cursor_content();
                ctx.term.cursor_col =  MIN( (ctx.term.cursor_col & ~7) + 8, ctx.term.WIDTH-1 );
                gfx_term_render_cursor();
                break;

            case 0x08:
            case 0x7F:
                /* backspace */
                if( ctx.term.cursor_col>0 )
                {
                    gfx_restore_cursor_content();
                    --ctx.term.cursor_col;
                    gfx_term_render_cursor();
                }
                break;

                 case 0x0e: // skip shift out
                 case 0x0f: // skip shift in
                 case 0x07: // skip BELL
                     break;

            case 0x0c:
                /* new page */
                gfx_term_move_cursor(0,0);
                gfx_term_clear_screen();
                break;

            default:
                ctx.term.state.next( *str, &(ctx.term.state) );
                break;
        }

        if( ctx.term.cursor_col >= ctx.term.WIDTH )
        {
            gfx_restore_cursor_content();
            ++ctx.term.cursor_row;
            ctx.term.cursor_col = 0;
            gfx_term_render_cursor();
        }

        if( ctx.term.cursor_row >= ctx.term.HEIGHT )
        {
            gfx_restore_cursor_content();
            --ctx.term.cursor_row;

            gfx_scroll_down_dma(ctx.font_height);
            gfx_term_render_cursor_newline_dma();
            dma_execute_queue();
        }

        ++str;
    }
}


void gfx_term_set_cursor_visibility( unsigned char visible )
{
    ctx.term.cursor_visible = visible;
}


void gfx_term_move_cursor( int row, int col )
{
    gfx_restore_cursor_content();

    ctx.term.cursor_row = MIN( ctx.term.HEIGHT-1, row>0 ? row : 0 );
    ctx.term.cursor_col = MIN( ctx.term.WIDTH-1, col>0 ? col : 0 );

    gfx_term_render_cursor();
}


void gfx_term_save_cursor()
{
    ctx.term.saved_cursor[0] = ctx.term.cursor_row;
    ctx.term.saved_cursor[1] = ctx.term.cursor_col;
}


void gfx_term_restore_cursor()
{
    gfx_restore_cursor_content();
    ctx.term.cursor_row = ctx.term.saved_cursor[0];
    ctx.term.cursor_col = ctx.term.saved_cursor[1];
    gfx_term_render_cursor();
}


void gfx_term_clear_till_end()
{
    gfx_restore_cursor_content();
    gfx_clear_rect( ctx.term.cursor_col*FONT_WIDTH, ctx.term.cursor_row*ctx.font_height, ctx.W, ctx.font_height );
    gfx_term_render_cursor();
}


void gfx_term_clear_till_cursor()
{
    gfx_restore_cursor_content();
    gfx_clear_rect( 0, ctx.term.cursor_row*ctx.font_height, (ctx.term.cursor_col+1)*FONT_WIDTH, ctx.font_height );
    gfx_term_render_cursor();
}


void gfx_term_clear_line()
{
    gfx_clear_rect( 0, ctx.term.cursor_row*ctx.font_height, ctx.W, ctx.font_height );
    gfx_term_render_cursor();
}


void gfx_term_clear_screen()
{
    gfx_clear();
    gfx_term_render_cursor();
}


void gfx_term_clear_lines(int from, int to)
{
    gfx_restore_cursor_content();
    if( from<0 ) {
        from = 0;
    }
    if( to>(int) ctx.term.HEIGHT-1 ) {
        to = ctx.term.HEIGHT-1;
    }
    if( from<=to ) {
        gfx_clear_rect(0, from*ctx.font_height, ctx.W, (to-from+1)*ctx.font_height);
        gfx_term_render_cursor();
    }
}


/*
 *  Term ANSI codes scanner
 *
 */
#define TERM_ESCAPE_CHAR (0x1B)


/*
 * State implementations
 */

void state_fun_final_letter( char ch, scn_state *state )
{
    if( state->private_mode_char == '#' ) {
        // Non-standard ANSI Codes
        switch( ch ) {
            case 'G':
                // GSX call
                if (state->cmd_params_size > 0) {
                    switch (state->cmd_params[0]) {
                        case OPC_OPEN_WORKSTATION:
                            gfx_open_workstation(state);
                            break;
                        case OPC_CLOSE_WORKSTATION:
                            gfx_close_workstation();
                            break;
                        case OPC_CLEAR_WORKSTATION:
                            gfx_clear_workstation();
                            break;
                        case OPC_UPDATE_WORKSTATION:
                            gfx_update_workstation();
                            break;
                        case OPC_ESCAPE:
                            if (state->cmd_params_size > 0) {
                                gfx_escape(state);
                            }
                            break;
                        case OPC_DRAW_POLYLINE:
                            if (state->cmd_params_size >= 6) {
                                gfx_draw_polyline(state);
                            }
                            break;
                        case OPC_DRAW_POLYMARKER:
                            gfx_draw_polymarkers(state);
                            break;
                        case OPC_DRAW_TEXT:
                            gfx_draw_text(state);
                            break;
                        case OPC_DRAW_FILLED_POLYGON:
                            if (state->cmd_params_size > 2 && state->cmd_params[1] > 2) {
                                gfx_filled_polygon(state);
                            }
                            break;
                        case OPC_DRAW_BITMAP:
                            gfx_draw_bitmap(state);
                            break;
                        case OPC_DRAWING_PRIMITIVE:
                            gfx_drawing_primitive(state);
                            break;
                        case OPC_TEXT_HEIGHT:
                            gfx_set_text_height(state);
                            break;
                        case OPC_TEXT_ROTATION:
                            if (state->cmd_params_size == 2) {
                                gfx_set_text_direction(state);
                            }
                            break;
                        case OPC_SET_PALETTE_COLOUR:
                            gfx_set_palette_colour(state);
                            break;
                        case OPC_POLYLINE_LINETYPE:
                            gsx.line_style = state->cmd_params[1];
                            break;
                        case OPC_POLYLINE_LINEWIDTH:
                            gsx.line_width = state->cmd_params[1];
                            break;
                        case OPC_POLYLINE_COLOUR:
                            gsx.line_colour = state->cmd_params[1];
                            break;
                        case OPC_POLYMARKER_TYPE:
                            gsx.marker_style = state->cmd_params[1];
                            break;
                        case OPC_POLYMARKER_HEIGHT:
                            gsx.marker_height = state->cmd_params[1];
                            break;
                        case OPC_POLYMARKER_COLOUR:
                            gsx.marker_colour = state->cmd_params[1];
                            break;
                        case OPC_TEXT_FONT:
                            gsx.text_font = state->cmd_params[1];
                            break;
                        case OPC_TEXT_COLOUR:
                            gsx.text_colour = state->cmd_params[1];
                            break;
                        case OPC_FILL_STYLE:
                            gsx.fill_style = state->cmd_params[1];
                            break;
                        case OPC_FILL_STYLE_INDEX:
                            gsx.fill_index = state->cmd_params[1];
                            break;
                        case OPC_FILL_COLOUR:
                            gsx.fill_colour = state->cmd_params[1];
                            break;
                        case OPC_GET_PALETTE_COLOUR:
                            break;
                        case OPC_GET_BITMAP:
                            gfx_get_bitmap(state);
                            break;
                        case OPC_INPUT_LOCATOR:
                            gfx_input_locator(state);
                            break;
                        case OPC_INPUT_VALUATOR:
                            gfx_input_valuator(state);
                            break;
                        case OPC_INPUT_CHOICE:
                            gfx_input_choice(state);
                            break;
                        case OPC_INPUT_STRING:
                            gfx_input_string(state);
                            break;
                        case OPC_WRITING_MODE:
                            gsx.writing_mode = state->cmd_params[1];
                            break;
                        case OPC_INPUT_MODE:
                            gsx_set_input_mode(state);
                            break;
                    }
                }
                goto back_to_normal;
                break;
            case 'm':
                // set either the default foreground or background colour
                if( state->cmd_params_size == 2 && state->cmd_params[1]<=WHITE) {
                    if( state->cmd_params[0] == 39 ) {
                        default_fg = state->cmd_params[1];
                    } else if( state->cmd_params[0] == 49 ) {
                        default_bg = state->cmd_params[1];
                    }
                }
                goto back_to_normal;
                break;
            case 'c':
                if( state->cmd_params_size == 1 ) {
                    gfx_putc( ctx.term.cursor_row, ctx.term.cursor_col, (char)(state->cmd_params[0]) );
                    ++ctx.term.cursor_col;
                    gfx_term_render_cursor();
                }
                goto back_to_normal;
                break;
        }
    }

    switch( ch ) {
        case 'l':
            if( state->private_mode_char == '?' &&
                state->cmd_params_size == 1 &&
                state->cmd_params[0] == 25 ) {
                gfx_term_set_cursor_visibility(0);
                gfx_restore_cursor_content();
            }
            goto back_to_normal;
            break;

        case 'h':
            if( state->private_mode_char == '?' &&
                state->cmd_params_size == 1 &&
                state->cmd_params[0] == 25 ) {
                gfx_term_set_cursor_visibility(1);
                gfx_term_render_cursor();
            }
            goto back_to_normal;
            break;

        case 'K':
            if( state->cmd_params_size== 0 ) {
                gfx_term_clear_till_end();
                goto back_to_normal;
            } else if( state->cmd_params_size== 1 ) {
                switch( state->cmd_params[0] ) {
                    case 0:
                        gfx_term_clear_till_end();
                        goto back_to_normal;
                    case 1:
                        gfx_term_clear_till_cursor();
                        goto back_to_normal;
                    case 2:
                        gfx_term_clear_line();
                        goto back_to_normal;
                    default:
                        goto back_to_normal;
                }
            }
            goto back_to_normal;
            break;

        case 'J':
            switch( state->cmd_params_size>=1 ? state->cmd_params[0] : 0 ) {
                case 0:
                    gfx_term_clear_till_end();
                    if( ctx.term.cursor_row+1 < ctx.term.HEIGHT ) {
                        gfx_term_clear_lines(ctx.term.cursor_row+1, ctx.term.HEIGHT-1);
                    }
                    break;

                case 1:
                    gfx_term_clear_till_cursor();
                    if( ctx.term.cursor_row>0 ) {
                        gfx_term_clear_lines(0, ctx.term.cursor_row-1);
                    }
                    break;

                case 2:
                    gfx_term_move_cursor(0, 0);
                    gfx_term_clear_screen();
                    break;
            }

            goto back_to_normal;
            break;

        case 'A':
            {
                int n = state->cmd_params_size > 0 ? state->cmd_params[0] : 1;
                gfx_term_move_cursor(MAX(0,(int) ctx.term.cursor_row-n), ctx.term.cursor_col);
                goto back_to_normal;
                break;
            }

        case 'B':
            {
                int n = state->cmd_params_size > 0 ? state->cmd_params[0] : 1;
                gfx_term_move_cursor(MIN((int) ctx.term.HEIGHT-1, (int) ctx.term.cursor_row+n), ctx.term.cursor_col);
                goto back_to_normal;
                break;
            }

        case 'C':
            {
                int n = state->cmd_params_size > 0 ? state->cmd_params[0] : 1;
                gfx_term_move_cursor(ctx.term.cursor_row, MIN((int) ctx.term.WIDTH-1, (int) ctx.term.cursor_col+n));
                goto back_to_normal;
                break;
            }

        case 'D':
            {
                int n = state->cmd_params_size > 0 ? state->cmd_params[0] : 1;
                gfx_term_move_cursor(ctx.term.cursor_row, MAX(0, (int) ctx.term.cursor_col-n));
                goto back_to_normal;
                break;
            }

        case 'M':
            // delete line(s)
            {
                int n = state->cmd_params_size > 0 ? state->cmd_params[0] : 1;
                gfx_restore_cursor_content();
                gfx_delete_lines(ctx.term.cursor_row, n);
                gfx_term_render_cursor();
                goto back_to_normal;
                break;
            }

        case 'L':
            // insert line(s)
            {
                int n = state->cmd_params_size > 0 ? state->cmd_params[0] : 1;
                gfx_restore_cursor_content();
                gfx_insert_lines(ctx.term.cursor_row, n);
                gfx_term_render_cursor();
                goto back_to_normal;
                break;
            }
        case 'Z':
            // backtab
            gfx_restore_cursor_content();
            if( ctx.term.cursor_col & 7 ) {
                ctx.term.cursor_col &= ~7;
            } else {
                ctx.term.cursor_col =  MAX( (int)ctx.term.cursor_col - 8, 0 );
            }
            gfx_term_render_cursor();
            break;

        case 'm':
            if( state->cmd_params_size==0 ) {
                gfx_term_reset_attrib();
            } else {
                unsigned int i;
                for(i=0; i<state->cmd_params_size; i++) {
                    if( i+2 < state->cmd_params_size && state->cmd_params[i]==38 && state->cmd_params[i+1]==5 ) {
                        i+=2; ctx.fg = state->cmd_params[i]; 
                    } else if( i+2 < state->cmd_params_size && state->cmd_params[i]==48 && state->cmd_params[i+1]==5 ){
                        i+=2; ctx.bg = state->cmd_params[i]; 
                    } else {
                        int p = state->cmd_params[i];
                        if( p==0 ) {
                            gfx_term_reset_attrib();
                        } else if( p==1 ) {
                            ctx.fg |= 8;                    // bold
                        } else if( p==2 ) {
                            ctx.fg &= 7;                    // normal
                        } else if( p==7 ) {
                            ctx.inverse = 1;                // reverse on
                        } else if( p==4 ) {
                            ctx.underline = 1;              // underline
                        } else if( p==27 ) {
                            ctx.inverse = 0;                // reverse off
                        } else if( p>=30 && p<=37 ) {
                            gfx_set_fg((ctx.fg&8) | (p-30));// fg colour
                        } else if( p==39 ) {
                            gfx_set_fg(default_fg);         // default fg
                        } else if( p>=40 && p<=47 ) {
                            gfx_set_bg(p-40);               // bg colour
                        } else if( p==49 ) {
                            gfx_set_bg(default_bg);         // default bg
                        }
                    }
                }
            }
            goto back_to_normal;
            break;

        case 'f':
        case 'H':
            {
                unsigned int r = state->cmd_params_size<1 ? 1 : state->cmd_params[0];
                unsigned int c = state->cmd_params_size<2 ? 1 : state->cmd_params[1];
                // if asked to move past the bottom of the screen, just move to the bottom
                // do the same sort of thing at the right side too.
                gfx_term_move_cursor(MIN(r-1, ctx.term.HEIGHT-1), MIN(c-1, ctx.term.WIDTH-1));
                goto back_to_normal;
                break;
            }

        case 's':
            gfx_term_save_cursor();
            goto back_to_normal;
            break;

        case 'u':
            gfx_term_restore_cursor();
            goto back_to_normal;
            break;

        case 'c':
            if( state->cmd_params_size == 0 || state->cmd_params[0]==0 ) {
                // according to: https://geoffg.net/Downloads/Terminal/VT100_User_Guide.pdf
                // query terminal type, respond "ESC [?1;Nc" where N is:
                // 0: Base VT100, no options
                // 1: Preprocessor option (STP)
                // 2: Advanced video option (AVO)
                // 3: AVO and STP
                // 4: Graphics processor option (GO)
                // 5: GO and STP
                // 6: GO and AVO
                // 7: GO, STP, and AVO
                char buf[7];
                buf[0] = '\033';
                buf[1] = '[';
                buf[2] = '?';
                buf[3] = '1';
                buf[4] = ';';
                buf[5] = '0';
                buf[6] = 'c';
                uart_write(buf, 7);
            }
            goto back_to_normal;
            break;

        case 'n':
            if( state->cmd_params_size == 1 ) {
                char buf[20];
                if( state->cmd_params[0] == 5 ) {
                    // query terminal status (always respond OK)
                    buf[0] = '\033';
                    buf[1] = '[';
                    buf[2] = '0';
                    buf[3] = 'n';
                    uart_write(buf, 4);
                } else if( state->cmd_params[0] == 6 ) {
                    // query cursor position
                    buf[0] = '\033';
                    buf[1] = '[';
                    b2s(buf+2, (unsigned char) ctx.term.cursor_row+1);
                    buf[5] = ';';
                    b2s(buf+6, (unsigned char) ctx.term.cursor_col+1);
                    buf[9] = 'R';
                    uart_write(buf, 10);
                }
            }

            goto back_to_normal;
            break;

        case '?':
            goto back_to_normal;
            break;

        default:
            goto back_to_normal;
    }

back_to_normal:
    // go back to normal text
    state->cmd_params_size = 0;
    state->next = state_fun_normaltext;
}

void state_fun_read_digit( char ch, scn_state *state )
{
    if( ch>='0' && ch <= '9' ) {
        // parse digit
        state->cmd_params[ state->cmd_params_size - 1] = state->cmd_params[ state->cmd_params_size - 1]*10 + (ch-'0');
        state->next = state_fun_read_digit; // stay on this state
        return;
    }

    if( ch == '\x00' ) {
        state->cmd_params[ state->cmd_params_size - 1] = state->cmd_params[ state->cmd_params_size - 1]*10;
        state->next = state_fun_read_digit; // stay on this state
        return;
    }

    if( ch == ';' ) {
        // Another param will follow
        state->cmd_params_size++;
        state->cmd_params[ state->cmd_params_size-1 ] = 0;
        state->next = state_fun_read_digit; // stay on this state
        return;
    }

    // not a digit, call the final state
    state_fun_final_letter( ch, state );
}

void state_fun_selectescape( char ch, scn_state *state )
{
    if( ch>='0' && ch<='9' ) {
        // start of a digit
        state->cmd_params_size = 1;
        state->cmd_params[ 0 ] = ch-'0';
        state->next = state_fun_read_digit;
        return;
    } else if( ch==';' ) {
        // assume an initial number of 0
        state->cmd_params_size = 2;
        state->cmd_params[0] = 0;
        state->cmd_params[1] = 0;
        state->next = state_fun_read_digit;
        return;
    } else if( ch=='?' || ch=='#' ) {
        state->private_mode_char = ch;

        // A digit will follow
        state->cmd_params_size = 1;
        state->cmd_params[ 0 ] = 0;
        state->next = state_fun_read_digit;
        return;
    }

    // Already at the final letter
    state_fun_final_letter( ch, state );

}

void state_fun_waitsquarebracket( char ch, scn_state *state )
{
    if( ch=='[' ) {
        state->cmd_params[0]=1;
        state->private_mode_char=0; // reset private mode char
        state->next = state_fun_selectescape;
        return;
    } else if( ch=='(' ) {
        state->next = state_fun_alt_charset;
        return;
    } else if( ch==TERM_ESCAPE_CHAR ) { // Double ESCAPE prints the ESC character
        gfx_putc( ctx.term.cursor_row, ctx.term.cursor_col, ch );
        ++ctx.term.cursor_col;
        gfx_term_render_cursor();
    } else if( ch=='c' ) {
        // ESC-c resets terminal
        gfx_term_reset_attrib();
        gfx_term_move_cursor(0,0);
        gfx_term_clear_screen();
    }

    state->next = state_fun_normaltext;
}

void state_fun_alt_charset( char ch, scn_state *state )
{
   if( ch=='0' ) {
      ctx.alt_charset = 1;
   } else if( ch=='B' ) {
      ctx.alt_charset = 0;
   }
   state->next = state_fun_normaltext;
}

void state_fun_normaltext( char ch, scn_state *state )
{
   if( ch==TERM_ESCAPE_CHAR ) {
      state->next = state_fun_waitsquarebracket;
      return;
   }

   if( ctx.alt_charset ) {
      switch( ch ) {
         case '}':
            ch = '\x9C';  // UK pound sign
            break;
         case '.':
            ch = '\x19';  // arrow pointing down
            break;
         case ',':
            ch = '\x1B';  // arrow pointing left
            break;
         case '+':
            ch = '\x1A';  // arrow pointing right
            break;
         case '-':
            ch = '\x18';  // arrow pointing up
            break;
         case 'h':
            ch = '\xB0';  // board of squares
            break;
         case '~':
            ch = '\x07';  // bullet
            break;
         case 'a':
            ch = '\xB1';  // checkerboard (stipple)
            break;
         case 'f':
            ch = '\xF8';  // degree symbol
            break;
         case '`':
            ch = '\x04';  // diamond
            break;
         case 'z':
            ch = '\xF2';  // greater than or equal to
            break;
         case '{':
            ch = '\xE3';  // greek Pi
            break;
         case 'q':
            ch = '\xC4';  // horizontal line
            break;
         case 'i':
            ch = '\x0F';  // lantern symbol
            break;
         case 'n':
            ch = '\xC5';  // large plus or crossover
            break;
         case 'y':
            ch = '\xF3';  // less than or equal to
            break;
         case 'm':
            ch = '\xC0';  // lower left corner
            break;
         case 'j':
            ch = '\xD9';  // lower right corner
            break;
         case '|':
            ch = '\xF7';  // not equal
            break;
         case 'g':
            ch = '\xF1';  // plus/minus
            break;
         case 'o':
            ch = '\xC4';  // scan line 1
            break;
         case 'p':
            ch = '\xC4';  // scan line 3
            break;
         case 'r':
            ch = '\xC4';  // scan line 7
            break;
         case 's':
            ch = '\xC4';  // scan line 9
            break;
         case '0':
            ch = '\xDB';  // solid square block
            break;
         case 'w':
            ch = '\xC2';  // tee pointing down
            break;
         case 'u':
            ch = '\xB4';  // tee pointing left
            break;
         case 't':
            ch = '\xC3';  // tee pointing right
            break;
         case 'v':
            ch = '\xC1';  // tee pointing up
            break;
         case 'l':
            ch = '\xDA';  // upper left corner
            break;
         case 'k':
            ch = '\xBF';  // upper right corner
            break;
         case 'x':
            ch = '\xB3';  // vertical line
            break;
      }
   }
   gfx_putc( ctx.term.cursor_row, ctx.term.cursor_col, ch );
   

   ++ctx.term.cursor_col;
   gfx_term_render_cursor();
}
