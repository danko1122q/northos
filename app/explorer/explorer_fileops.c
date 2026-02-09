#include "explorer.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"

void confirmNewFolder(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK || state.dialog_input == -1)
		return;

	char newDir[MAX_LONG_STRLEN];
	strcpy(newDir, state.desktop.widgets[state.dialog_input].context.inputfield->text);

	if (strlen(newDir) == 0) {
		clearDialog();
		return;
	}

	char fullPath[MAX_LONG_STRLEN];
	int pathLen = strlen(state.current_path);

	if (pathLen > 0 && pathLen + strlen(newDir) + 2 < MAX_LONG_STRLEN) {
		strcpy(fullPath, state.current_path);
		strcat(fullPath, "/");
		strcat(fullPath, newDir);
	} else if (pathLen == 0 && strlen(newDir) < MAX_LONG_STRLEN) {
		strcpy(fullPath, newDir);
	} else {
		clearDialog();
		return;
	}

	mkdir(fullPath);
	clearDialog();
	state.need_refresh = 1;
}

void confirmNewFile(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK || state.dialog_input == -1)
		return;

	char newFile[MAX_LONG_STRLEN];
	strcpy(newFile, state.desktop.widgets[state.dialog_input].context.inputfield->text);

	// Trim whitespace
	int i = 0, j = 0;
	while (newFile[i] == ' ') i++;
	while (newFile[i] != '\0') {
		newFile[j++] = newFile[i++];
	}
	newFile[j] = '\0';

	if (strlen(newFile) == 0) {
		clearDialog();
		return;
	}

	char fullPath[MAX_LONG_STRLEN];
	int pathLen = strlen(state.current_path);

	if (pathLen > 0 && pathLen + strlen(newFile) + 2 < MAX_LONG_STRLEN) {
		strcpy(fullPath, state.current_path);
		strcat(fullPath, "/");
		strcat(fullPath, newFile);
	} else if (pathLen == 0 && strlen(newFile) < MAX_LONG_STRLEN) {
		strcpy(fullPath, newFile);
	} else {
		clearDialog();
		return;
	}

	int fd = open(fullPath, O_CREATE | O_WRONLY);
	if (fd >= 0)
		close(fd);

	clearDialog();
	state.need_refresh = 1;
}

// Fungsi helper untuk copy file
int copyFile(char *src, char *dst) {
	int fd_src = open(src, O_RDONLY);
	if (fd_src < 0)
		return -1;

	int fd_dst = open(dst, O_CREATE | O_WRONLY);
	if (fd_dst < 0) {
		close(fd_src);
		return -1;
	}

	char buf[512];
	int n;
	while ((n = read(fd_src, buf, sizeof(buf))) > 0) {
		if (write(fd_dst, buf, n) != n) {
			close(fd_src);
			close(fd_dst);
			return -1;
		}
	}

	close(fd_src);
	close(fd_dst);
	return 0;
}

void confirmRename(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK || state.dialog_input == -1)
		return;
	if (state.selected_index == -1 || state.selected_index >= state.total_items)
		return;

	char newName[MAX_SHORT_STRLEN];
	strcpy(newName, state.desktop.widgets[state.dialog_input].context.inputfield->text);

	// Trim whitespace
	int i = 0, j = 0;
	while (newName[i] == ' ') i++;
	while (newName[i] != '\0') {
		newName[j++] = newName[i++];
	}
	newName[j] = '\0';

	if (strlen(newName) == 0 || strcmp(newName, state.selected_name) == 0) {
		clearDialog();
		return;
	}

	char oldPath[MAX_LONG_STRLEN], newPath[MAX_LONG_STRLEN];
	int pathLen = strlen(state.current_path);

	// Bangun path lengkap
	if (pathLen > 0) {
		if (pathLen + strlen(state.selected_name) + 2 >= MAX_LONG_STRLEN ||
		    pathLen + strlen(newName) + 2 >= MAX_LONG_STRLEN) {
			clearDialog();
			return;
		}
		strcpy(oldPath, state.current_path);
		strcat(oldPath, "/");
		strcat(oldPath, state.selected_name);
		strcpy(newPath, state.current_path);
		strcat(newPath, "/");
		strcat(newPath, newName);
	} else {
		if (strlen(state.selected_name) >= MAX_LONG_STRLEN ||
		    strlen(newName) >= MAX_LONG_STRLEN) {
			clearDialog();
			return;
		}
		strcpy(oldPath, state.selected_name);
		strcpy(newPath, newName);
	}

	if (state.is_selected_dir) {
		// Untuk directory: coba buat directory baru
		if (mkdir(newPath) == 0) {
			// Directory baru berhasil dibuat
			// Hapus directory lama (hanya jika kosong)
			// Jika ada isi, unlink akan gagal dan directory lama tetap ada
			unlink(oldPath);
		}
	} else {
		// Untuk file: gunakan link() atau copy
		if (link(oldPath, newPath) == 0) {
			// Link berhasil, hapus yang lama
			unlink(oldPath);
		} else {
			// Link gagal, coba copy
			if (copyFile(oldPath, newPath) == 0) {
				unlink(oldPath);
			}
		}
	}

	clearDialog();
	state.selected_index = -1;
	state.need_refresh = 1;
}