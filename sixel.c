/* See LICENSE for license details. */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "st.h"
#include "sixel.h"

#define SIXEL_RGB(r, g, b) (0xFF000000 | ((r) << 16) | ((g) << 8) | (b))
#define DECSIXEL_PALETTE_MAX 256

static ImageList *images_head = NULL;

void
sixel_init(void)
{
	sixel_clear_images();
}

void
sixel_clear_images(void)
{
	ImageList *img, *next;
	
	for (img = images_head; img != NULL; img = next) {
		next = img->next;
		free(img->data);
		free(img);
	}
	images_head = NULL;
}

ImageList *
sixel_get_images(void)
{
	return images_head;
}

void
sixel_parser_init(SixelState *state)
{
	int i;

	memset(state, 0, sizeof(SixelState));
	state->transparent = -1;
	
	/* Initialize default VT340 color palette */
	state->palette[0] = SIXEL_RGB(0, 0, 0);       /* Black */
	state->palette[1] = SIXEL_RGB(51, 102, 179);  /* Blue */
	state->palette[2] = SIXEL_RGB(179, 51, 51);   /* Red */
	state->palette[3] = SIXEL_RGB(51, 179, 51);   /* Green */
	state->palette[4] = SIXEL_RGB(179, 51, 179);  /* Magenta */
	state->palette[5] = SIXEL_RGB(51, 179, 179);  /* Cyan */
	state->palette[6] = SIXEL_RGB(179, 179, 51);  /* Yellow */
	state->palette[7] = SIXEL_RGB(179, 179, 179); /* White */
	state->palette[8] = SIXEL_RGB(102, 102, 102); /* Gray */
	state->palette[9] = SIXEL_RGB(102, 153, 255); /* Light Blue */
	state->palette[10] = SIXEL_RGB(255, 102, 102); /* Light Red */
	state->palette[11] = SIXEL_RGB(102, 255, 102); /* Light Green */
	state->palette[12] = SIXEL_RGB(255, 102, 255); /* Light Magenta */
	state->palette[13] = SIXEL_RGB(102, 255, 255); /* Light Cyan */
	state->palette[14] = SIXEL_RGB(255, 255, 102); /* Light Yellow */
	state->palette[15] = SIXEL_RGB(255, 255, 255); /* Bright White */
	
	for (i = 16; i < DECSIXEL_PALETTE_MAX; i++)
		state->palette[i] = SIXEL_RGB(255, 255, 255);
	
	state->ncolors = 16;
	state->color = 0;
}

void
sixel_parser_deinit(SixelState *state)
{
	free(state->data);
	state->data = NULL;
	state->datasize = 0;
}

static void
sixel_ensure_capacity(SixelState *state, int size)
{
	if (size > state->datasize) {
		state->datasize = size + 4096;
		state->data = xrealloc(state->data, state->datasize);
	}
}

static void
sixel_put_pixel(SixelState *state, int x, int y, uint32_t color)
{
	int offset;

	if (x < 0 || y < 0)
		return;
	
	if (x >= state->width)
		state->width = x + 1;
	if (y >= state->height)
		state->height = y + 1;
	
	offset = (y * state->width + x) * 4;
	sixel_ensure_capacity(state, offset + 4);
	
	state->data[offset + 0] = (color >> 16) & 0xFF; /* R */
	state->data[offset + 1] = (color >> 8) & 0xFF;  /* G */
	state->data[offset + 2] = color & 0xFF;         /* B */
	state->data[offset + 3] = (color >> 24) & 0xFF; /* A */
}

int
sixel_parse_dcs(SixelState *state, const char *seq, int len)
{
	int i, n, val, r, g, b;
	const char *p = seq;
	const char *end = seq + len;
	char c;

	/* Skip DCS header and parameters until 'q' */
	while (p < end && *p != 'q')
		p++;
	
	if (p >= end || *p != 'q')
		return -1;
	
	p++; /* skip 'q' */

	while (p < end) {
		c = *p++;
		
		if (c == '\033' || c == '\0')
			break;
		
		/* Raster attributes */
		if (c == '"') {
			/* Format: " Pan; Pad; Ph; Pv */
			/* We can skip these for basic implementation */
			while (p < end && *p != ';' && *p != '-' && (*p < '?' || *p > '~'))
				p++;
			continue;
		}
		
		/* Color definition: # Pc; Pu; Px; Py; Pz */
		if (c == '#') {
			val = 0;
			while (p < end && isdigit(*p))
				val = val * 10 + (*p++ - '0');
			
			if (val >= 0 && val < DECSIXEL_PALETTE_MAX) {
				state->color = val;
				
				if (p < end && *p == ';') {
					p++;
					/* Color format type (skip) */
					while (p < end && isdigit(*p))
						p++;
					
					if (p < end && *p == ';') {
						p++;
						/* RGB values */
						r = 0;
						while (p < end && isdigit(*p))
							r = r * 10 + (*p++ - '0');
						
						if (p < end && *p == ';') {
							p++;
							g = 0;
							while (p < end && isdigit(*p))
								g = g * 10 + (*p++ - '0');
							
							if (p < end && *p == ';') {
								p++;
								b = 0;
								while (p < end && isdigit(*p))
									b = b * 10 + (*p++ - '0');
								
								/* Convert from 0-100 to 0-255 */
								r = (r * 255) / 100;
								g = (g * 255) / 100;
								b = (b * 255) / 100;
								
								state->palette[val] = SIXEL_RGB(r, g, b);
								if (val >= state->ncolors)
									state->ncolors = val + 1;
							}
						}
					}
				}
			}
			continue;
		}
		
		/* Graphics repeat: ! Pn c */
		if (c == '!') {
			n = 0;
			while (p < end && isdigit(*p))
				n = n * 10 + (*p++ - '0');
			
			if (n < 1)
				n = 1;
			
			if (p < end && *p >= '?' && *p <= '~') {
				c = *p++;
				val = c - '?';
				
				for (i = 0; i < 6; i++) {
					if (val & (1 << i)) {
						int x, y;
						for (x = 0; x < n; x++)
							sixel_put_pixel(state, state->x + x, state->y + i,
							                state->palette[state->color]);
					}
				}
				state->x += n;
			}
			continue;
		}
		
		/* Carriage return */
		if (c == '$') {
			state->x = 0;
			continue;
		}
		
		/* Line feed */
		if (c == '-') {
			state->x = 0;
			state->y += 6;
			continue;
		}
		
		/* Sixel data */
		if (c >= '?' && c <= '~') {
			val = c - '?';
			
			for (i = 0; i < 6; i++) {
				if (val & (1 << i))
					sixel_put_pixel(state, state->x, state->y + i,
					                state->palette[state->color]);
			}
			state->x++;
		}
	}

	return 0;
}

ImageList *
sixel_get_image(SixelState *state, int col, int row)
{
	ImageList *img;
	
	if (state->width <= 0 || state->height <= 0 || !state->data)
		return NULL;
	
	/* Allocate new image */
	img = xmalloc(sizeof(ImageList));
	img->data = state->data;
	img->width = state->width;
	img->height = state->height;
	img->x = col;
	img->y = row;
	img->reflow = 0;
	img->next = images_head;
	images_head = img;
	
	/* Transfer ownership of data to image */
	state->data = NULL;
	state->datasize = 0;
	
	return img;
}
