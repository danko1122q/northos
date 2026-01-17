#ifndef ICONS_H
#define ICONS_H

/* --- Icon Dimensions & Capacity --- */
#define ICON_SIZE   30   // Each icon is a 30x30 pixel square
#define ICON_NUMBER 5    // Total number of icons defined in the data array

/* --- Icon Index Mapping --- */
// These constants serve as indices into the 'icons_data' 2D array.
#define ICON_CLOSE    0  // Close button (usually an 'X')
#define ICON_MINIMIZE 1  // Minimize button (usually a '_')
#define ICON_WINDOWS  2  // Logo or start menu icon
#define ICON_FOLDER   3  // File system directory icon
#define ICON_CHESS    4  // Specific application icon (Chess game)

/* --- Color Definitions --- */
// Defines the Alpha/Color value used for transparency.
// 0x00000000 represents fully transparent pixels in an ARGB8888 format,
// where the high byte (Alpha channel) is 0x00.
#define ICON_TRANSPARENT 0x00000000

/* --- Global Data Buffer --- */
// 'extern' indicates that the actual pixel data is defined in another file 
// (likely icons.c). It is a 2D array where icons_data[index] returns 
// an array of 900 (30*30) unsigned integers (colors).
extern unsigned int icons_data[ICON_NUMBER][ICON_SIZE * ICON_SIZE];

#endif
