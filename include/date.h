#ifndef DATE_H
#define DATE_H

#include "types.h" // Tambahkan ini agar 'uint' dikenali

struct rtcdate {
	uint second;
	uint minute;
	uint hour;
	uint day;
	uint month;
	uint year;
};

#endif // DATE_H