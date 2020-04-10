#ifndef _GFX_H_
#define _GFX_H_

#include "gfx_types.h"

extern void gfx_set_env( void* p_framebuffer, unsigned int width, unsigned int height, unsigned int pitch, unsigned int size ); 
extern void gfx_set_bg( GFX_COL col );
extern void gfx_set_fg( GFX_COL col );
extern void gfx_swap_fg_bg();
extern void gfx_get_term_size( unsigned int* rows, unsigned int* cols );
extern void gfx_set_font_height(unsigned int h);
extern void gfx_toggle_font_height();
extern void gfx_toggle_lines();

/*! 
 * Fills the entire framebuffer with the background color 
 */
extern void gfx_clear();


/*! 
 * Fills a rectangle with the foreground color 
 */
extern void gfx_fill_rect( unsigned int x, unsigned int y, unsigned int width, unsigned int height );

/*! 
 * Renders a line from x0-y0 to x1-y1 
 */
extern void gfx_line( int x0, int y0, int x1, int y1, GFX_COL clr );

/*! 
 * Renders a filled line from x0-y to x1-y 
 */
extern void gfx_filled_hline( int x0, int x1, int y, GFX_COL clr);

/*! 
 * Renders a circle of radius r at x0-y0
 */
extern void gfx_circle( int x0, int y0, int r, GFX_COL clr, boolean filled );

/*! 
 * Renders a pixel atm x-y
 */
extern void gfx_pixel( int x, int y, GFX_COL clr );

/*! 
 * Fills a rectangle with the background color 
 */
extern void gfx_clear_rect( unsigned int x, unsigned int y, unsigned int width, unsigned int height );

extern void gfx_open_workstation(scn_state *state);
extern void gfx_close_workstation();
extern void gfx_clear_workstation();
extern void gfx_update_workstation();
extern void gfx_escape(scn_state *state);
extern void gfx_place_graphic_cursor(int x, int y);
extern void gfx_remove_graphic_cursor(void);
extern void gfx_read_rect(int ulx, int uly, int w, int h,unsigned char* rect);
extern void gfx_write_rect(int ulx, int uly, int w, int h,unsigned char* rect);
extern void gfx_draw_polyline(scn_state *state);
extern void gfx_draw_polymarkers(scn_state *state);
extern void gfx_draw_text(scn_state *state);
extern void gfx_filled_polygon(scn_state *state);
extern void gfx_circle(int x0, int y0, int r, GFX_COL clr, boolean filled);
extern float atan(float z);
extern float atan2(int y, int x);
extern void gfx_draw_arc_point(int x0, int y0, int x, int y, int a0, int a1, GFX_COL clr);
extern void gfx_draw_arc(int x0, int y0, int r, int arc_s, int arc_e, GFX_COL clr);
extern void gfx_draw_bitmap(scn_state *state);
extern void gfx_drawing_primitive(scn_state *state);
extern void gfx_set_text_height(scn_state *state);
extern void gfx_set_text_direction(scn_state *state);
extern void gfx_set_palette_colour(scn_state *state);
extern void gfx_get_bitmap(scn_state *state);
extern void gfx_input_locator(scn_state *state);
extern void gfx_input_valuator(scn_state *state);
extern void gfx_input_choice(scn_state *state);
extern void gfx_input_string(scn_state *state);
extern void gsx_set_input_mode(scn_state *state);
/*! 
 * Renders the character "c" at location (x,y)
 */
extern void gfx_putc( unsigned int row, unsigned int col, unsigned char c );


/*! 
 * Scrolls the entire framebuffer down (adding background color at the bottom)
 */
extern void gfx_scroll_down( unsigned int npixels );


/*! 
 * Scrolls the entire framebuffer up (adding background color at the top)
 */
extern void gfx_scroll_up( unsigned int npixels );



/*!
 *  Terminal
 *
 */
extern void gfx_term_putstring( const char* str );
extern void gfx_term_set_cursor_visibility( unsigned char visible );
extern void gfx_term_move_cursor( int row, int col );
extern void gfx_term_save_cursor();
extern void gfx_term_restore_cursor();
extern void gfx_term_clear_till_end();
extern void gfx_term_clear_till_cursor();
extern void gfx_term_clear_line();
extern void gfx_term_clear_lines(int from, int to);
extern void gfx_term_clear_screen();
extern void gfx_term_reset_attrib();


#endif
