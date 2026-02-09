#include "explorer.h"
#include "user.h"
#include "fs.h"
#include "stat.h"

ExplorerState state;

void initColors() {
	state.colors.color_bg.R = 248;
	state.colors.color_bg.G = 249;
	state.colors.color_bg.B = 250;
	state.colors.color_bg.A = 255;
	
	state.colors.color_topbar.R = 255;
	state.colors.color_topbar.G = 255;
	state.colors.color_topbar.B = 255;
	state.colors.color_topbar.A = 255;
	
	state.colors.color_statusbar.R = 245;
	state.colors.color_statusbar.G = 246;
	state.colors.color_statusbar.B = 247;
	state.colors.color_statusbar.A = 255;

	state.colors.color_text_dark.R = 33;
	state.colors.color_text_dark.G = 37;
	state.colors.color_text_dark.B = 41;
	state.colors.color_text_dark.A = 255;
	
	state.colors.color_text_light.R = 108;
	state.colors.color_text_light.G = 117;
	state.colors.color_text_light.B = 125;
	state.colors.color_text_light.A = 255;

	state.colors.color_accent.R = 0;
	state.colors.color_accent.G = 123;
	state.colors.color_accent.B = 255;
	state.colors.color_accent.A = 255;
	
	state.colors.color_folder.R = 52;
	state.colors.color_folder.G = 144;
	state.colors.color_folder.B = 220;
	state.colors.color_folder.A = 255;
	
	state.colors.color_file.R = 108;
	state.colors.color_file.G = 117;
	state.colors.color_file.B = 125;
	state.colors.color_file.A = 255;

	// Selection box - lebih transparan
	state.colors.color_selected.R = 0;
	state.colors.color_selected.G = 123;
	state.colors.color_selected.B = 255;
	state.colors.color_selected.A = 25;  // Dikurangi dari 40 jadi 25 untuk lebih transparan
	
	state.colors.color_border.R = 222;
	state.colors.color_border.G = 226;
	state.colors.color_border.B = 230;
	state.colors.color_border.A = 255;
	
	state.colors.color_input_border.R = 206;
	state.colors.color_input_border.G = 212;
	state.colors.color_input_border.B = 218;
	state.colors.color_input_border.A = 255;

	state.colors.color_dialog_bg.R = 255;
	state.colors.color_dialog_bg.G = 255;
	state.colors.color_dialog_bg.B = 255;
	state.colors.color_dialog_bg.A = 255;
	
	state.colors.color_dialog_overlay.R = 0;
	state.colors.color_dialog_overlay.G = 0;
	state.colors.color_dialog_overlay.B = 0;
	state.colors.color_dialog_overlay.A = 120;

	state.colors.color_button.R = 0;
	state.colors.color_button.G = 123;
	state.colors.color_button.B = 255;
	state.colors.color_button.A = 255;
	
	state.colors.color_white.R = 255;
	state.colors.color_white.G = 255;
	state.colors.color_white.B = 255;
	state.colors.color_white.A = 255;
}

char *getFileExtension(char *filename) {
	static char buf[DIRSIZ + 1];
	char *p;

	if (!filename || strlen(filename) == 0)
		return "";

	for (p = filename + strlen(filename); p >= filename && *p != '.'; p--)
		;
	p++;

	if (strlen(p) >= DIRSIZ)
		return p;

	memmove(buf, p, strlen(p));
	memset(buf + strlen(p), '\0', 1);
	return buf;
}

// Cek apakah file adalah executable/binary
int isExecutable(char *filename) {
    if (!filename)
        return 0;

    // 1. Cek daftar nama file binary yang sudah pasti executable (tanpa ekstensi)
    char *executables[] = {
        "terminal", "editor", "explorer", "floppybird", "calculator"
    };

    int numExecs = sizeof(executables) / sizeof(executables[0]);
    for (int i = 0; i < numExecs; i++) {
        if (strcmp(filename, executables[i]) == 0)
            return 1; // Ini pasti binary
    }

    char *ext = getFileExtension(filename);

    // 2. Jika tidak punya ekstensi dan tidak ada di daftar binary di atas, 
    //    tapi punya huruf besar (seperti README), biasanya itu teks.
    if (strlen(ext) == 0) {
        // Cek apakah ada huruf besar, jika ya, kemungkinan besar file teks dokumentasi
        for (int i = 0; filename[i] != '\0'; i++) {
            if (filename[i] >= 'A' && filename[i] <= 'Z') return 0;
        }
        return 1; // Sisanya tanpa ekstensi dianggap binary
    }

    // 3. Daftar ekstensi teks/source code yang aman (BUKAN Executable)
    const char *safeExtensions[] = {
        "txt", "md", "json", "xml", "yaml", "yml", "ini", "conf", "csv", "log",
        "html", "htm", "css", "js", "ts", "jsx", "tsx", "php", "asp", "jsp",
        "c", "cpp", "h", "hpp", "cc", "hh", "cxx", "s", "asm",
        "java", "kt", "kts", "groovy", "scala",
        "py", "rb", "pl", "sh", "bat", "ps1", "lua", "r", "dart",
        "go", "rs", "swift", "sql", "toml", "lock"
    };

    int numSafe = sizeof(safeExtensions) / sizeof(safeExtensions[0]);

    for (int i = 0; i < numSafe; i++) {
        if (strcmp(ext, safeExtensions[i]) == 0) {
            return 0; //Mengembalikan 0 (Bukan Executable)
        }
    }

    // 4. Jika punya ekstensi tapi tidak terdaftar di 'safe', anggap binary (e.g. .bin, .exe, .o)
    return 1; 
}

int shouldShowFile(char *filename) {
	if (!filename)
		return 0;

	// 1. Daftar file sistem/perintah internal yang ingin disembunyikan
	char *systemFiles[] = {
		".", "..", "kernel", "initcode", "init", 
		"cat", "echo", "grep", "kill", "ln", "ls", 
		"mkdir", "rm", "sh", "wc", 
		"usertests", "forktest", "desktop",
	};

	int numSystemFiles = sizeof(systemFiles) / sizeof(systemFiles[0]);

	for (int i = 0; i < numSystemFiles; i++) {
		if (strcmp(filename, systemFiles[i]) == 0) {
			return 0; // Jangan tampilkan file ini
		}
	}

	// 2. Tampilkan file yang tidak ada di daftar cekal di atas
	return 1;
}

int canOpenWithEditor(char *filename) {
	if (!filename)
		return 0;

	// Jika executable, jangan buka dengan editor
	if (isExecutable(filename))
		return 0;

	char *ext = getFileExtension(filename);
	
	// File dengan extension teks
	if (strcmp(ext, "txt") == 0 || 
	    strcmp(ext, "md") == 0 ||
	    strcmp(ext, "c") == 0 ||
	    strcmp(ext, "h") == 0 ||
	    strcmp(ext, "asm") == 0 ||
	    strcmp(ext, "s") == 0)
		return 1;

	// File dengan huruf kapital (biasanya README, LICENSE, dll)
	for (int i = 0; filename[i] != '\0'; i++) {
		if (filename[i] >= 'A' && filename[i] <= 'Z')
			return 1;
	}

	// File tanpa extension yang bukan executable
	if (strlen(ext) == 0 && !isExecutable(filename))
		return 1;

	return 0;
}

int isProtected(char *name) {
	if (!name)
		return 1;

	char *protected[] = {"editor", "explorer", "terminal", "floppybird",
			     ".", "..", "kernel", "initcode",
			     "init", "cat", "echo", "grep",
			     "kill", "ln", "ls", "mkdir",
			     "rm", "sh", "wc", "stressfs",
			     "usertests", "zombie", "forktest"};

	for (int i = 0; i < 23; i++) {
		if (strcmp(name, protected[i]) == 0)
			return 1;
	}
	return 0;
}

char *getParentPath(char *path) {
	static char buf[MAX_LONG_STRLEN];
	char *p;

	if (!path) {
		strcpy(buf, "/");
		return buf;
	}

	for (p = path + strlen(path); p >= path && *p != '/'; p--)
		;

	int len = p - path;
	if (len >= MAX_LONG_STRLEN)
		len = MAX_LONG_STRLEN - 1;

	memmove(buf, path, len);
	buf[len] = '\0';

	if (strlen(buf) == 0)
		strcpy(buf, "/");
	return buf;
}

char *fmtname(char *path) {
	static char buf[DIRSIZ + 1];
	char *p;

	if (!path)
		return "";

	for (p = path + strlen(path); p >= path && *p != '/'; p--)
		;
	p++;

	if (strlen(p) >= DIRSIZ)
		return p;

	memmove(buf, p, strlen(p));
	memset(buf + strlen(p), '\0', 1);
	return buf;
}