/*
 * gfx_types.h
 *
 *  Created on: Nov 14, 2018
 *      Author: bkg2018
 */

#ifndef SRC_GFX_TYPES_H_
#define SRC_GFX_TYPES_H_

/** Color type */
typedef unsigned char GFX_COL;
typedef int boolean;

typedef struct SCN_STATE
{
    //state_fun* next;
    void (*next)( char ch, struct SCN_STATE *state );
    void (*after_read_digit)( char ch, struct SCN_STATE *state );

    unsigned int cmd_params[644];
    unsigned int cmd_params_size;
    char private_mode_char;
} scn_state;

/** Drawing modes for putc and putsprite */
typedef enum
{
	drawingNORMAL,			// foreground color on background color (normal text mode)
	drawingXOR,				// pixel xored with background (text and sprites)
	drawingTRANSPARENT,		// non null pixel drawn on background (text and sprites)
} DRAWING_MODE;

/** Standard color codes */
typedef enum
{
	BLACK			= 0x00,
	DARKRED		= 0x01,
	DARKGREEN	= 0x02,
	DARKYELLOW	= 0x03,
	DARKBLUE		= 0x04,
	DARKMAGENTA	= 0x05,
	DARKCYAN		= 0x06,
	GRAY			= 0x07,
	DARKGRAY		= 0x08,
	RED			= 0x09,
	GREEN			= 0x0A,
	YELLOW		= 0x0B,
	BLUE			= 0x0C,
	MAGENTA		= 0x0D,
	CYAN			= 0x0E,
	WHITE			= 0x0F
} DRAWING_COLOR; // compatible with GFX_COL

#define DEFAULT_FG DARKGREEN
#define DEFAULT_BG BLACK

// Function type for the functions drawing a character in each mode (normal, xor, transparent)
typedef void draw_putc_fun( unsigned int row, unsigned int col, unsigned char c );

// Function type for the functions drawing a sprite in each mode (normal, xor, transparent)
typedef void draw_putsprite_fun( unsigned char* p_sprite, unsigned int x, unsigned int y );

#define	GSX_MAX_NDC		32767
#define	MAX_PTSIN		75		// This is what CP/M-80 had (all the room
										// it had in GDOS for internal buffer).
										// If needed, we can expand arbitrarly.
// GSX opcodes
enum {
  OPC_OPEN_WORKSTATION = 1,
  OPC_CLOSE_WORKSTATION = 2,
  OPC_CLEAR_WORKSTATION = 3,
  OPC_UPDATE_WORKSTATION = 4,
  OPC_ESCAPE = 5,
  OPC_DRAW_POLYLINE = 6,
  OPC_DRAW_POLYMARKER = 7,
  OPC_DRAW_TEXT = 8,
  OPC_DRAW_FILLED_POLYGON = 9,
  OPC_DRAW_BITMAP = 10,
  OPC_DRAWING_PRIMITIVE = 11,
  OPC_TEXT_HEIGHT = 12,
  OPC_TEXT_ROTATION = 13,
  OPC_SET_PALETTE_COLOUR = 14,
  OPC_POLYLINE_LINETYPE = 15,
  OPC_POLYLINE_LINEWIDTH = 16,
  OPC_POLYLINE_COLOUR = 17,
  OPC_POLYMARKER_TYPE = 18,
  OPC_POLYMARKER_HEIGHT = 19,
  OPC_POLYMARKER_COLOUR = 20,
  OPC_TEXT_FONT = 21,
  OPC_TEXT_COLOUR = 22,
  OPC_FILL_STYLE = 23,
  OPC_FILL_STYLE_INDEX = 24,
  OPC_FILL_COLOUR = 25,
  OPC_GET_PALETTE_COLOUR = 26,
  OPC_GET_BITMAP = 27,
  OPC_INPUT_LOCATOR = 28,
  OPC_INPUT_VALUATOR = 29,
  OPC_INPUT_CHOICE = 30,
  OPC_INPUT_STRING = 31,
  OPC_WRITING_MODE = 32,
  OPC_INPUT_MODE = 33
};

// Escape opcodes
enum {
  ESC_GET_TEXT_ROWS_AND_COLUMNS = 1,
  ESC_EXIT_GRAPHICS_MODE = 2,
  ESC_ENTER_GRAPHICS_MODE = 3,
  ESC_TEXT_CURSOR_UP = 4,
  ESC_TEXT_CURSOR_DOWN = 5,
  ESC_TEXT_CURSOR_RIGHT = 6,
  ESC_TEXT_CURSOR_LEFT = 7,
  ESC_TEXT_CURSOR_HOME = 8,
  ESC_TEXT_ERASE_EOS = 9,
  ESC_TEXT_ERASE_EOL = 10,
  ESC_SET_TEXT_CURSOR = 11,
  ESC_TEXT = 12,
  ESC_TEXT_REVERSE_VIDEO_ON = 13,
  ESC_TEXT_REVERSE_VIDEO_OFF = 14,
  ESC_GET_TEXT_CURSOR = 15,
  ESC_TABLET_STATUS = 16,
  ESC_HARDCOPY = 17,
  ESC_PLACE_GRAPHICS_CURSOR = 18,
  ESC_REMOVE_GRAPHICS_CURSOR = 19,
};

// Drawing primitives
enum {
  DRAW_BAR = 1,
  DRAW_ARC = 2,
  DRAW_PIE_SLICE = 3,
  DRAW_CIRCLE = 4,
  DRAW_GRAPHICS_CHAR = 5
};

enum {
	GSX_PB_CONTRL = 0,
	GSX_PB_INTIN,
	GSX_PB_PTSIN,
	GSX_PB_INTOUT,
	GSX_PB_PTSOUT,
	
	GSX_PB_MAX,
};

enum {	
	CONTRL_FUNCTION = 0, // function
	CONTRL_PTSIN, 
	CONTRL_PTSOUT,	
	CONTRL_INTIN,
	CONTRL_INTOUT,
	CONTRL_SPECIAL
};

enum {
	CONTROL_FUNCTIONID = 5,
};

enum {	
	OWS_ID = 0,
	OWS_LINETYPE,
	OWS_POLYLINE_COLOR_INDEX,
	OWS_MARKER_TYPE,
	OWS_POLYMARKER_COLOR_INDEX,
	OWS_TEXT_FONT,
	OWS_TEXT_COLOR_INDEX,
	OWS_FILL_INTERIOR_STYLE,
	OWS_FILL_STYLE_INDEX,
	OWS_FILL_COLOR_INDEX
};

enum {
	OWS_INTOUT_MAX_WIDTH,
	OWS_INTOUT_MAX_HEIGHT,
	OWS_INTOUT_COORD_FLAGS,
	OWS_INTOUT_PIXEL_WIDTH,
	OWS_INTOUT_PIXEL_HEIGHT
};

enum {
	LS_SOLID = 1,
	LS_DASH,
	LS_DOT,
	LS_DASH_DOT,
	LS_LONG_DASH
};

static const unsigned int line_style_mask[] = {
	0,
	0b1111111111111111,
	0b1111111100000000,
	0b1110000011100000,
	0b1111111000111000,
	0b1111111111110000
};

enum {
	MS_DOT = 1,
	MS_PLUS,
	MS_ASTERISK,
	MS_CIRCLE,
	MS_CROSS
};

enum {
	FS_HOLLOW = 0,
	FS_SOLID,
	FS_HALFTONE,
	FS_HATCH
};

enum {
	FSI_VERTICAL = 1,
	FSI_HORIZONTAL,
	FSI_PLUS45,
	FSI_MINUS45,
	FSI_CROSS,
	FSI_X
};

static const unsigned long halftone1[] = {
	0xAAAAAAAA, 0x00000000, 0xA888A888, 0x00000000,
	0xAAAAAAAA, 0x00000000, 0x88888888, 0x00000000,
	0xAAAAAAAA, 0x00000000, 0xA8A888A8, 0x00000000,
	0xAAAAAAAA, 0x00000000, 0x88888888, 0x00000000,
	0xAAAAAAAA, 0x00000000, 0xA888A888, 0x00000000,
	0xAAAAAAAA, 0x00000000, 0x88888888, 0x00000000,
	0xAAAAAAAA, 0x00000000, 0x88A8A8A8, 0x00000000,
	0xAAAAAAAA, 0x00000000, 0x88888888, 0x00000000
};

static const unsigned long halftone2[] = {
	0x11111111, 0xAAAAAAAA, 0x44444445, 0xAAAAAAAA,
	0x11111111, 0xAAAAAAAA, 0x54545454, 0xAAAAAAAA,
	0x11111111, 0xAAAAAAAA, 0x45444544, 0xAAAAAAAA,
	0x11111111, 0xAAAAAAAA, 0x54545454, 0xAAAAAAAA,
	0x11111111, 0xAAAAAAAA, 0x44454445, 0xAAAAAAAA,
	0x11111111, 0xAAAAAAAA, 0x54545454, 0xAAAAAAAA,
	0x11111111, 0xAAAAAAAA, 0x45444544, 0xAAAAAAAA,
	0x11111111, 0xAAAAAAAA, 0x54545454, 0xAAAAAAAA
};

static const unsigned long halftone3[] = {
	0x55555555, 0xABABABAB, 0x55555555, 0xEEEEEEEE,
	0x55555555, 0xBABABABA, 0x55555555, 0xEEEEEEEE,
	0x55555555, 0xABABABAB, 0x55555555, 0xEEEEEEEE,
	0x55555555, 0xBBBABBBA, 0x55555555, 0xEEEEEEEE,
	0x55555555, 0xABABABAB, 0x55555555, 0xEEEEEEEE,
	0x55555555, 0xBABBBABA, 0x55555555, 0xEEEEEEEE,
	0x55555555, 0xABABABAB, 0x55555555, 0xEEEEEEEE,
	0x55555555, 0xBBBABBBA, 0x55555555, 0xEEEEEEEE
};

static const unsigned long halftone4[] = {
	0x55575557, 0xFFFFFFFF, 0xDDDDDDDD, 0xFFFFFFFF,
	0x75757575, 0xFFFFFFFF, 0xDDDDDDDD, 0xFFFFFFFF,
	0x57555757, 0xFFFFFFFF, 0xDDDDDDDD, 0xFFFFFFFF,
	0x75757575, 0xFFFFFFFF, 0xDDDDDDDD, 0xFFFFFFFF,
	0x55575557, 0xFFFFFFFF, 0xDDDDDDDD, 0xFFFFFFFF,
	0x75757575, 0xFFFFFFFF, 0xDDDDDDDD, 0xFFFFFFFF,
	0x57575755, 0xFFFFFFFF, 0xDDDDDDDD, 0xFFFFFFFF,
	0x75757575, 0xFFFFFFFF, 0xDDDDDDDD, 0xFFFFFFFF
};

enum {
	WM_REPLACE = 1,
	WM_TRANSPARENT,
	WM_XOR,
	WM_ERASE
};

enum {
	LID_LOCATOR = 1,
	LID_VALUATOR,
	LID_CHOICE,
	LID_STRING,
};
#define LID_MAX 4

enum {
	IM_REQUEST = 1,
	IM_SAMPLE
};

#define TW_MIN	7
#define TH_MIN 12

#define GSX_GC_ARMLNG	10
#define GSX_GC_W			(2 * GSX_GC_ARMLNG + 1)

typedef struct GSX_STATE {
	int	device_number;
	int	line_style;
	int	line_colour;
	int	marker_style;
	int	marker_colour;
	int	text_style;
	int	text_colour;
	int	fill_style;
	int	fill_index;
	int	fill_colour;
	int	text_height;
	int	text_width;
	int	text_size;
	int	text_direction;
	int	text_font;
	int	line_width;
	int	marker_height;
	int	writing_mode;
	int	input_mode[LID_MAX];
	int	reverse_video;
	unsigned char gc_rect[GSX_GC_W * GSX_GC_W];
	int gc_ulx, gc_uly, gc_w, gc_h;
} gsx_state;

#endif /* SRC_GFX_TYPES_H_ */
