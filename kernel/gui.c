#include "gui.h"
#include "character.h"
#include "defs.h"
#include "icons.h"
#include "memlayout.h"
#include "mmu.h"
#include "mouse_shape.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "types.h"
#include "x86.h"

/* Global screen properties */
ushort SCREEN_WIDTH;
ushort SCREEN_HEIGHT;
int screen_size;

/* Buffer pointers for direct memory access and off-screen rendering */
RGB *screen;
RGB *screen_buf;

/**
 * Initializes the Graphical User Interface (GUI) environment.
 * Maps the physical video memory, reads display dimensions from VESA/BIOS,
 * and initializes the window manager.
 */
void initGUI() {
	// Points to graphic memory information passed by the bootloader
	uint GraphicMem = KERNBASE + 0x1028;

	uint baseAdd = *((uint *)GraphicMem);
	screen = (RGB *)baseAdd;

	// Read dimensions from KERNEL BASE offsets (VESA Information)
	SCREEN_WIDTH = *((ushort *)(KERNBASE + 0x1012));
	SCREEN_HEIGHT = *((ushort *)(KERNBASE + 0x1014));

	screen_size =
		(SCREEN_WIDTH * SCREEN_HEIGHT) * 3; // 3 bytes per pixel (RGB)

	// Set secondary buffer address immediately after the primary screen
	// memory
	screen_buf = (RGB *)(baseAdd + screen_size);

	// Initialize default mouse cursor colors
	mouse_color[0].G = 0;
	mouse_color[0].B = 0;
	mouse_color[0].R = 0;
	mouse_color[1].G = 200;
	mouse_color[1].B = 200;
	mouse_color[1].R = 200;

	// Diagnostic output for kernel debugging
	cprintf("KERNEL BASE ADDRESS: %x\n", KERNBASE);
	cprintf("SCREEN PHYSICAL ADDRESS: %x\n", baseAdd);
	cprintf("@Screen Width:   %d\n", SCREEN_WIDTH);
	cprintf("@Screen Height:  %d\n", SCREEN_HEIGHT);
	cprintf("@Video card drivers initialized successfully.\n");

	wmInit();
}

/**
 * Draws a single pixel by directly copying RGB values.
 */
void drawPoint(RGB *color, RGB origin) {
	color->R = origin.R;
	color->G = origin.G;
	color->B = origin.B;
}

/**
 * Draws a pixel with alpha blending support.
 * Uses a linear interpolation formula: Result = Background * (1 - Alpha) +
 * Source * Alpha
 */
void drawPointAlpha(RGB *color, RGBA origin) {
	float alpha;
	// Opaque: skip calculations
	if (origin.A == 255) {
		color->R = origin.R;
		color->G = origin.G;
		color->B = origin.B;
		return;
	}
	// Fully transparent: skip drawing
	if (origin.A == 0) {
		return;
	}

	alpha = (float)origin.A / 255.0f;
	color->R = (uchar)(color->R * (1.0f - alpha) + origin.R * alpha);
	color->G = (uchar)(color->G * (1.0f - alpha) + origin.G * alpha);
	color->B = (uchar)(color->B * (1.0f - alpha) + origin.B * alpha);
}

/**
 * Renders a single character from the font bitmap.
 * Includes clipping to prevent out-of-bounds memory access.
 */
int drawCharacter(RGB *buf, int x, int y, char ch, RGBA color) {
	int i, j;
	RGB *t;
	int ord = ch - 0x20; // Map ASCII to font array index

	if (ord < 0 || ord >= (CHARACTER_NUMBER - 1))
		return -1;

	for (i = 0; i < CHARACTER_HEIGHT; i++) {
		// Vertical Clipping
		if (y + i >= SCREEN_HEIGHT || y + i < 0)
			continue;

		for (j = 0; j < CHARACTER_WIDTH; j++) {
			// Retrieve font intensity (alpha) from font data array
			// (0-255)
			uchar font_alpha = character[ord][i][j];

			if (font_alpha > 0) {
				// Horizontal Clipping
				if (x + j >= SCREEN_WIDTH || x + j < 0)
					continue;

				// Calculate buffer offset
				t = buf + (y + i) * SCREEN_WIDTH + x + j;

				// ANTI-ALIASING KEY:
				// Blend the user-defined color alpha with the
				// font's inherent antialiasing alpha
				RGBA smooth_color = color;
				smooth_color.A = (color.A * font_alpha) / 255;

				drawPointAlpha(t, smooth_color);
			}
		}
	}
	return CHARACTER_WIDTH;
}

/**
 * Renders an icon from predefined icon data.
 * Skips specific color keys to simulate transparency.
 */
int drawIcon(RGB *buf, int x, int y, int icon, RGBA color) {
	int i, j;
	RGB *t;

	if (icon < 0 || icon >= ICON_NUMBER) {
		return -1;
	}

	for (i = 0; i < ICON_SIZE; i++) {
		if (y + i >= SCREEN_HEIGHT || y + i < 0)
			continue;

		for (j = 0; j < ICON_SIZE; j++) {
			if (x + j >= SCREEN_WIDTH || x + j < 0)
				continue;

			unsigned int raw_color =
				icons_data[icon][i * ICON_SIZE + j];

			/* * TRANSPARENCY CHECK: Skip pixel if it matches:
			 * 1. 0xFF000000 (Opaque Black used as transparency key
			 * in icons_data.c)
			 * 2. 0x00000000 (Standard transparent)
			 * 3. ICON_TRANSPARENT (Definition from icons.h)
			 */
			if (raw_color == 0xFF000000 ||
			    raw_color == 0x00000000 ||
			    raw_color == ICON_TRANSPARENT) {
				continue;
			}

			t = buf + (y + i) * SCREEN_WIDTH + (x + j);

			RGBA pixel_color;
			pixel_color.R = (raw_color >> 16) & 0xFF;
			pixel_color.G = (raw_color >> 8) & 0xFF;
			pixel_color.B = raw_color & 0xFF;
			pixel_color.A = 255;

			drawPointAlpha(t, pixel_color);
		}
	}
	return ICON_SIZE;
}

/**
 * Renders a standard null-terminated string.
 */
void drawString(RGB *buf, int x, int y, char *str, RGBA color) {
	int offset_x = 0;
	while (*str != '\0') {
		offset_x += drawCharacter(buf, x + offset_x, y, *str, color);
		str++;
	}
}

/**
 * Renders a string but stops if the next character exceeds the specified width.
 */
void drawStringWithMaxWidth(RGB *buf, int x, int y, int width, char *str,
			    RGBA color) {
	int offset_x = 0;
	while (*str != '\0' && offset_x + CHARACTER_WIDTH <= width) {
		offset_x += drawCharacter(buf, x + offset_x, y, *str, color);
		str++;
	}
}

/**
 * Renders an RGBA image buffer to the screen buffer.
 */
void drawImage(RGB *buf, RGBA *img, int x, int y, int width, int height,
	       int max_x, int max_y) {
	int i, j;
	RGB *t;
	RGBA *o;
	for (i = 0; i < height; i++) {
		if (y + i >= max_y)
			break;
		if (y + i < 0)
			continue;
		for (j = 0; j < width; j++) {
			if (x + j >= max_x)
				break;
			if (x + j < 0)
				continue;

			t = buf + (y + i) * SCREEN_WIDTH + x + j;
			o = img + (height - i) * width +
			    j; // Vertical flip if needed
			drawPointAlpha(t, *o);
		}
	}
}

/**
 * Renders a 24-bit RGB image using fast memory move (memmove).
 */
void draw24Image(RGB *buf, RGB *img, int x, int y, int width, int height,
		 int max_x, int max_y) {
	int i;
	RGB *t;
	RGB *o;
	int max_line = (max_x - x) < width ? (max_x - x) : width;
	for (i = 0; i < height; i++) {
		if (y + i >= max_y)
			break;
		if (y + i < 0)
			continue;

		t = buf + (y + i) * SCREEN_WIDTH + x;
		o = img + (height - i) * width;
		memmove(t, o, max_line * 3);
	}
}

/**
 * Renders a specific rectangular sub-part of an RGB image.
 */
void draw24ImagePart(RGB *buf, RGB *img, int x, int y, int width, int height,
		     int subx, int suby, int subw, int subh) {
	if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
		return;

	int minj = x < 0 ? -x : 0;
	int maxj = x + subw > SCREEN_WIDTH ? SCREEN_WIDTH - x : subw;
	if (minj >= maxj)
		return;

	int i;
	RGB *t;
	RGB *o;
	for (i = 0; i < subh; i++) {
		if (y + i < 0)
			continue;
		if (y + i >= SCREEN_HEIGHT)
			break;

		t = buf + (y + i) * SCREEN_WIDTH + minj + x;
		o = img + (i + suby) * width + subx + minj;
		memmove(t, o, (maxj - minj) * 3);
	}
}

/**
 * Draws a filled rectangle with alpha blending and boundary clipping.
 */
void drawRectBound(RGB *buf, int x, int y, int width, int height, RGBA fill,
		   int max_x, int max_y) {
	int i, j;
	RGB *t;
	for (i = 0; i < height; i++) {
		if (y + i < 0)
			continue;
		if (y + i >= max_y)
			break;
		for (j = 0; j < width; j++) {
			if (x + j < 0)
				continue;
			if (x + j >= max_x)
				break;

			t = buf + (y + i) * SCREEN_WIDTH + x + j;
			drawPointAlpha(t, fill);
		}
	}
}

/**
 * Draws the outline (border) of a rectangle.
 */
void drawRectBorder(RGB *buf, RGB color, int x, int y, int width, int height) {
	if (x >= SCREEN_WIDTH || x + width < 0 || y >= SCREEN_HEIGHT ||
	    y + height < 0 || width < 0 || height < 0)
		return;

	int i;
	RGB *t = buf + y * SCREEN_WIDTH + x;

	// Top border
	if (y > 0) {
		for (i = 0; i < width; i++) {
			if (x + i > 0 && x + i < SCREEN_WIDTH)
				*(t + i) = color;
		}
	}
	// Bottom border
	if (y + height < SCREEN_HEIGHT) {
		RGB *o = t + height * SCREEN_WIDTH;
		for (i = 0; i < width; i++) {
			if (y > 0 && x + i > 0 && x + i < SCREEN_WIDTH)
				*(o + i) = color;
		}
	}
	// Left border
	if (x > 0) {
		for (i = 0; i < height; i++) {
			if (y + i > 0 && y + i < SCREEN_HEIGHT)
				*(t + i * SCREEN_WIDTH) = color;
		}
	}
	// Right border
	if (x + width < SCREEN_WIDTH) {
		RGB *o = t + width;
		for (i = 0; i < height; i++) {
			if (y + i > 0 && y + i < SCREEN_HEIGHT)
				*(o + i * SCREEN_WIDTH) = color;
		}
	}
}

/**
 * General function to draw a filled rectangle on the whole screen.
 */
void drawRect(RGB *buf, int x, int y, int width, int height, RGBA fill) {
	drawRectBound(buf, x, y, width, height, fill, SCREEN_WIDTH,
		      SCREEN_HEIGHT);
}

/**
 * Draws a rectangle using coordinate pairs (min_x, min_y) to (max_x, max_y).
 */
void drawRectByCoord(RGB *buf, int xmin, int ymin, int xmax, int ymax,
		     RGBA fill) {
	drawRect(buf, xmin, ymin, xmax - xmin, ymax - ymin, fill);
}

/**
 * Clears a specific area of the screen by restoring it from a background
 * buffer.
 */
void clearRect(RGB *buf, RGB *temp_buf, int x, int y, int width, int height) {
	RGB *t;
	RGB *o;
	int i;
	int max_line = (SCREEN_WIDTH - x) < width ? (SCREEN_WIDTH - x) : width;
	for (i = 0; i < height; i++) {
		if (y + i >= SCREEN_HEIGHT)
			break;
		if (y + i < 0)
			continue;

		t = buf + (y + i) * SCREEN_WIDTH + x;
		o = temp_buf + (y + i) * SCREEN_WIDTH + x;
		memmove(t, o, max_line * 3);
	}
}

/**
 * Coordinates-based clearing of an area.
 */
void clearRectByCoord(RGB *buf, RGB *temp_buf, int xmin, int ymin, int xmax,
		      int ymax) {
	clearRect(buf, temp_buf, xmin, ymin, xmax - xmin, ymax - ymin);
}

/**
 * Renders the mouse cursor at the given (x, y) coordinates.
 */
void drawMouse(RGB *buf, int mode, int x, int y) {
	int i, j;
	RGB *t;
	for (i = 0; i < MOUSE_HEIGHT; i++) {
		if (y + i >= SCREEN_HEIGHT)
			break;
		if (y + i < 0)
			continue;
		for (j = 0; j < MOUSE_WIDTH; j++) {
			if (x + j >= SCREEN_WIDTH)
				break;
			if (x + j < 0)
				continue;

			uchar temp = mouse_pointer[mode][i][j];
			if (temp) {
				t = buf + (y + i) * SCREEN_WIDTH + x + j;
				drawPoint(t, mouse_color[temp - 1]);
			}
		}
	}
}

/**
 * Removes the mouse cursor by clearing its last known position.
 */
void clearMouse(RGB *buf, RGB *temp_buf, int x, int y) {
	clearRect(buf, temp_buf, x, y, MOUSE_WIDTH, MOUSE_HEIGHT);
}