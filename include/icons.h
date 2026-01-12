#ifndef ICONS_H
#define ICONS_H

#define ICON_SIZE 30
#define ICON_NUMBER 5

#define ICON_CLOSE 0
#define ICON_MINIMIZE 1
#define ICON_WINDOWS 2
#define ICON_FOLDER 3
#define ICON_CHESS 4

// Ganti dari 0xFF000000 menjadi 0x00000000
#define ICON_TRANSPARENT 0x00000000

extern unsigned int icons_data[ICON_NUMBER][ICON_SIZE * ICON_SIZE];

#endif