#include "pigfx_config.h"
#include "gfx.h"
#include "console.h"
#include "dma.h"
#include "utils.h"
#include "uart.h"
#include "ee_printf.h"
#include <stdio.h>

extern unsigned char G_FONT_GLYPHS08;
extern unsigned char G_FONT_GLYPHS14;
extern unsigned char G_FONT_GLYPHS16;

// default FONT_HEIGHT is configurable in pigfx_config.h but 
// FONT_WIDTH is fixed at 8 for implementation efficiency
// (one character line fits exactly in two 32-bit integers)
#define FONT_WIDTH 8


#define MIN( v1, v2 ) ( ((v1) < (v2)) ? (v1) : (v2))
#define MAX( v1, v2 ) ( ((v1) > (v2)) ? (v1) : (v2))
#define PFB( X, Y ) ( ctx.pfb + Y*ctx.Pitch + X )

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


typedef struct SCN_STATE
{
    //state_fun* next;
    void (*next)( char ch, struct SCN_STATE *state );
    void (*after_read_digit)( char ch, struct SCN_STATE *state );

    unsigned int cmd_params[10];
    unsigned int cmd_params_size;
    char private_mode_char;
} scn_state;

/* state forward declarations */
typedef void state_fun( char ch, scn_state *state );
void state_fun_normaltext( char ch, scn_state *state );
void state_fun_read_digit( char ch, scn_state *state );



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
    gfx_fill_rect(x,y,width,height);
    ctx.fg = curr_fg;
}


void gfx_line( int x0, int y0, int x1, int y1 )
{
    x0 = MAX( MIN(x0, (int)ctx.W), 0 );
    y0 = MAX( MIN(y0, (int)ctx.H), 0 );
    x1 = MAX( MIN(x1, (int)ctx.W), 0 );
    y1 = MAX( MIN(y1, (int)ctx.H), 0 );

    unsigned char qrdt = __abs__(y1 - y0) > __abs__(x1 - x0);

    if( qrdt ) {
        __swap__(&x0, &y0);
        __swap__(&x1, &y1);
    }
    if( x0 > x1 ) {
        __swap__(&x0, &x1);
        __swap__(&y0, &y1);
    }

    const int deltax = x1 - x0;
    const int deltay = __abs__(y1 - y0);
    register int error = deltax >> 1;
    register unsigned char* pfb;
    unsigned int nr = x1-x0;

    if( qrdt )
    {
        const int ystep = y0<y1 ? 1 : -1;
        pfb = PFB(y0,x0);
        while(nr--)
        {
              *pfb = GET_FG(ctx);
            error = error - deltay;
            if( error < 0 )
            {
                pfb += ystep;
                error += deltax;
            }
            pfb += ctx.Pitch;
        }
    }
    else
    {
        const int ystep = y0<y1 ? ctx.Pitch : -ctx.Pitch;
        pfb = PFB(x0,y0);
        while(nr--)
        {
              *pfb = GET_FG(ctx);
            error = error - deltay;
            if( error < 0 )
            {
                pfb += ystep;
                error += deltax;
            }
            pfb++;
        }
    }
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
                ctx.term.cursor_col += 1;
                ctx.term.cursor_col =  MIN( ctx.term.cursor_col + FONT_WIDTH - ctx.term.cursor_col%FONT_WIDTH, ctx.term.WIDTH-1 );
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
            case 'l':
                // render line
                if( state->cmd_params_size == 4 ) {
                    gfx_line( state->cmd_params[0], state->cmd_params[1], state->cmd_params[2], state->cmd_params[3] );
                }
                goto back_to_normal;
                break;
            case 'r':
                // render a filled rectangle
                if( state->cmd_params_size == 4 ) {
                    gfx_fill_rect( state->cmd_params[0], state->cmd_params[1], state->cmd_params[2], state->cmd_params[3] );
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
                int r = state->cmd_params_size<1 ? 1 : state->cmd_params[0];
                int c = state->cmd_params_size<2 ? 1 : state->cmd_params[1];
                gfx_term_move_cursor((r-1) % ctx.term.HEIGHT, (c-1) % ctx.term.WIDTH);
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
                    // query terminal status (always responde OK)
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

void state_fun_normaltext( char ch, scn_state *state )
{
    if( ch==TERM_ESCAPE_CHAR ) {
        state->next = state_fun_waitsquarebracket;
        return;
    }

    gfx_putc( ctx.term.cursor_row, ctx.term.cursor_col, ch );
    ++ctx.term.cursor_col;
    gfx_term_render_cursor();
}
