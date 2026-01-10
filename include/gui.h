#ifndef GUI_H
#define GUI_H

#include "types.h" // WAJIB: Agar 'ushort' dikenali oleh compiler

#define GUI_BUF 0x9000

#ifndef __ASSEMBLER__

// Variabel eksternal untuk resolusi layar
extern ushort SCREEN_WIDTH;
extern ushort SCREEN_HEIGHT;
extern int screen_size;

// 24 bit RGB. Digunakan untuk buffer warna dasar
typedef struct RGB {
	unsigned char B;
	unsigned char G;
	unsigned char R;
} RGB;

// 32 bit RGBA. Digunakan untuk warna dengan transparansi (Alpha)
typedef struct RGBA {
	unsigned char A;
	unsigned char B;
	unsigned char G;
	unsigned char R;
} RGBA;

#endif // __ASSEMBLER__

#endif // GUI_H