/* See LICENSE for license details. */

#ifndef SIXEL_H
#define SIXEL_H

#include <stdint.h>

/* Sixel image structure */
typedef struct ImageList {
	unsigned char *data;  /* RGBA pixel data */
	int width;
	int height;
	int x;                /* terminal column position */
	int y;                /* terminal row position */
	int cw;               /* cell width in pixels */
	int ch;               /* cell height in pixels */
	int reflow;           /* should reflow on resize */
	struct ImageList *next;
} ImageList;

/* Sixel parser state */
typedef struct {
	int color;            /* current color */
	int x;                /* current x position */
	int y;                /* current y position */
	int width;            /* image width */
	int height;           /* image height */
	unsigned char *data;  /* image data buffer */
	int datasize;         /* allocated size */
	uint32_t palette[256]; /* color palette (RGBA) */
	int ncolors;          /* number of colors defined */
	int transparent;      /* transparent color index (-1 if none) */
} SixelState;

/* Function prototypes */
void sixel_init(void);
void sixel_parser_init(SixelState *state);
void sixel_parser_deinit(SixelState *state);
int sixel_parse_dcs(SixelState *state, const char *seq, int len);
ImageList *sixel_get_image(SixelState *state, int col, int row);
ImageList *sixel_get_images(void);
void sixel_clear_images(void);

#endif /* SIXEL_H */
