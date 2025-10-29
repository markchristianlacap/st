/* See LICENSE for license details. */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "st.h"
#include "sixel.h"

#define SIXEL_RGB(r, g, b) (0xFF000000 | ((r) << 16) | ((g) << 8) | (b))
#define DECSIXEL_PALETTE_MAX 256
#define SIXEL_MAX_WIDTH 4096
#define SIXEL_MAX_HEIGHT 4096

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
	
	/* Pre-allocate a reasonable buffer */
	state->datasize = SIXEL_MAX_WIDTH * 100 * 4; /* Start with 100 rows */
	state->data = xmalloc(state->datasize);
	memset(state->data, 0, state->datasize);
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
		int newsize = state->datasize;
		while (newsize < size)
			newsize *= 2;
		state->data = xrealloc(state->data, newsize);
		memset(state->data + state->datasize, 0, newsize - state->datasize);
		state->datasize = newsize;
	}
}

static void
sixel_put_pixel(SixelState *state, int x, int y, uint32_t color)
{
	int offset;

	if (x < 0 || y < 0)
		return;
	
	/* Limit to reasonable size */
	if (x >= SIXEL_MAX_WIDTH || y >= SIXEL_MAX_HEIGHT)
		return;
	
	/* Track maximum dimensions */
	if (x >= state->width)
		state->width = x + 1;
	if (y >= state->height)
		state->height = y + 1;
	
	/* Use fixed maximum width for offset calculation to avoid reorganization */
	offset = (y * SIXEL_MAX_WIDTH + x) * 4;
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
			
			fprintf(stderr, "sixel: Drawing sixel char '%c' (val=%d) at x=%d, y=%d, color=%d\n",
			        c, val, state->x, state->y, state->color);
			
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
	unsigned char *compacted;
	int x, y, src_offset, dst_offset;
	
	fprintf(stderr, "sixel_get_image: width=%d, height=%d, data=%p\n",
	        state->width, state->height, (void*)state->data);
	
	if (state->width <= 0 || state->height <= 0 || !state->data)
		return NULL;
	
	/* Compact the data from SIXEL_MAX_WIDTH stride to actual width */
	compacted = xmalloc(state->width * state->height * 4);
	for (y = 0; y < state->height; y++) {
		for (x = 0; x < state->width; x++) {
			src_offset = (y * SIXEL_MAX_WIDTH + x) * 4;
			dst_offset = (y * state->width + x) * 4;
			compacted[dst_offset + 0] = state->data[src_offset + 0];
			compacted[dst_offset + 1] = state->data[src_offset + 1];
			compacted[dst_offset + 2] = state->data[src_offset + 2];
			compacted[dst_offset + 3] = state->data[src_offset + 3];
		}
	}
	
	/* Allocate new image */
	img = xmalloc(sizeof(ImageList));
	img->data = compacted;
	img->width = state->width;
	img->height = state->height;
	img->x = col;
	img->y = row;
	img->reflow = 0;
	img->next = images_head;
	images_head = img;
	
	/* Keep the parser data for potential reuse */
	return img;
}
