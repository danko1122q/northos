#include "fcntl.h"
#include "fs.h"
#include "gui.h"
#include "memlayout.h"
#include "msg.h"
#include "stat.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_handler.h"
#include "user_window.h"

#define TOPBAR_HEIGHT 40
#define STATUSBAR_HEIGHT 24
#define ITEM_HEIGHT 24
#define ITEM_PADDING 4
#define CONTENT_PADDING 8
#define MAX_PATH_DEPTH 256

window desktop;
char current_path[MAX_LONG_STRLEN];

int topbar_widget = -1;
int statusbar_widget = -1;
int path_display_widget = -1;

int selected_index = -1;
int total_items = 0;
char selected_name[MAX_SHORT_STRLEN];
int is_selected_dir = 0;

int dialog_active = 0;
int dialog_bg = -1;
int dialog_panel = -1;
int dialog_input = -1;
int dialog_btn1 = -1;
int dialog_btn2 = -1;
int dialog_title = -1;

int need_refresh = 0;

typedef struct {
	char name[MAX_SHORT_STRLEN];
	int is_dir;
	int widget_id;
	int y_pos;
} FileItem;

FileItem items[MAX_WIDGET_SIZE];

struct RGBA color_bg;
struct RGBA color_topbar;
struct RGBA color_statusbar;
struct RGBA color_text_dark;
struct RGBA color_text_light;
struct RGBA color_accent;
struct RGBA color_folder;
struct RGBA color_file;
struct RGBA color_selected;
struct RGBA color_border;
struct RGBA color_dialog_bg;
struct RGBA color_dialog_overlay;
struct RGBA color_button;
struct RGBA color_white;
struct RGBA color_input_border;

void initColors() {
	color_bg.R = 248;
	color_bg.G = 249;
	color_bg.B = 250;
	color_bg.A = 255;
	color_topbar.R = 255;
	color_topbar.G = 255;
	color_topbar.B = 255;
	color_topbar.A = 255;
	color_statusbar.R = 245;
	color_statusbar.G = 246;
	color_statusbar.B = 247;
	color_statusbar.A = 255;

	color_text_dark.R = 33;
	color_text_dark.G = 37;
	color_text_dark.B = 41;
	color_text_dark.A = 255;
	color_text_light.R = 108;
	color_text_light.G = 117;
	color_text_light.B = 125;
	color_text_light.A = 255;

	color_accent.R = 0;
	color_accent.G = 123;
	color_accent.B = 255;
	color_accent.A = 255;
	color_folder.R = 52;
	color_folder.G = 144;
	color_folder.B = 220;
	color_folder.A = 255;
	color_file.R = 108;
	color_file.G = 117;
	color_file.B = 125;
	color_file.A = 255;

	color_selected.R = 0;
	color_selected.G = 123;
	color_selected.B = 255;
	color_selected.A = 40;
	color_border.R = 222;
	color_border.G = 226;
	color_border.B = 230;
	color_border.A = 255;
	color_input_border.R = 206;
	color_input_border.G = 212;
	color_input_border.B = 218;
	color_input_border.A = 255;

	color_dialog_bg.R = 255;
	color_dialog_bg.G = 255;
	color_dialog_bg.B = 255;
	color_dialog_bg.A = 255;
	color_dialog_overlay.R = 0;
	color_dialog_overlay.G = 0;
	color_dialog_overlay.B = 0;
	color_dialog_overlay.A = 120;

	color_button.R = 0;
	color_button.G = 123;
	color_button.B = 255;
	color_button.A = 255;
	color_white.R = 255;
	color_white.G = 255;
	color_white.B = 255;
	color_white.A = 255;
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

int shouldShowFile(char *filename) {
	if (!filename)
		return 0;

	char *guiApps[] = {"terminal", "editor", "explorer", "floppybird"};
	for (int i = 0; i < 4; i++) {
		if (strcmp(filename, guiApps[i]) == 0)
			return 1;
	}

	char *ext = getFileExtension(filename);
	if (strcmp(ext, "txt") == 0 || strcmp(ext, "md") == 0)
		return 1;

	for (int i = 0; filename[i] != '\0'; i++) {
		if (filename[i] >= 'A' && filename[i] <= 'Z')
			return 1;
	}
	return 0;
}

int canOpenWithEditor(char *filename) {
	if (!filename)
		return 0;

	char *ext = getFileExtension(filename);
	if (strcmp(ext, "txt") == 0 || strcmp(ext, "md") == 0)
		return 1;

	for (int i = 0; filename[i] != '\0'; i++) {
		if (filename[i] >= 'A' && filename[i] <= 'Z')
			return 1;
	}
	return 0;
}

int isProtected(char *name) {
	if (!name)
		return 1;

	char *protected[] = {"editor",	  "explorer", "terminal", "floppybird",
			     ".",	  "..",	      "kernel",	  "initcode",
			     "init",	  "cat",      "echo",	  "grep",
			     "kill",	  "ln",	      "ls",	  "mkdir",
			     "rm",	  "sh",	      "wc",	  "stressfs",
			     "usertests", "zombie",   "forktest"};

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

void clearDialog() {
	if (dialog_bg != -1) {
		removeWidget(&desktop, dialog_bg);
		dialog_bg = -1;
	}
	if (dialog_panel != -1) {
		removeWidget(&desktop, dialog_panel);
		dialog_panel = -1;
	}
	if (dialog_title != -1) {
		removeWidget(&desktop, dialog_title);
		dialog_title = -1;
	}
	if (dialog_input != -1) {
		removeWidget(&desktop, dialog_input);
		dialog_input = -1;
	}
	if (dialog_btn1 != -1) {
		removeWidget(&desktop, dialog_btn1);
		dialog_btn1 = -1;
	}
	if (dialog_btn2 != -1) {
		removeWidget(&desktop, dialog_btn2);
		dialog_btn2 = -1;
	}
	dialog_active = 0;
	desktop.needsRepaint = 1;
}

void cancelDialog(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearDialog();
	}
}

void confirmNewFolder(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK || dialog_input == -1)
		return;

	char newDir[MAX_LONG_STRLEN];
	strcpy(newDir, desktop.widgets[dialog_input].context.inputfield->text);

	if (strlen(newDir) == 0) {
		clearDialog();
		return;
	}

	char fullPath[MAX_LONG_STRLEN];
	int pathLen = strlen(current_path);

	if (pathLen > 0 && pathLen + strlen(newDir) + 2 < MAX_LONG_STRLEN) {
		strcpy(fullPath, current_path);
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
	need_refresh = 1;
}

void confirmNewFile(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK || dialog_input == -1)
		return;

	char newFile[MAX_LONG_STRLEN];
	strcpy(newFile, desktop.widgets[dialog_input].context.inputfield->text);

	if (strlen(newFile) == 0) {
		clearDialog();
		return;
	}

	char fullPath[MAX_LONG_STRLEN];
	int pathLen = strlen(current_path);

	if (pathLen > 0 && pathLen + strlen(newFile) + 2 < MAX_LONG_STRLEN) {
		strcpy(fullPath, current_path);
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
	need_refresh = 1;
}

void confirmRename(Widget *w, message *msg) {
	if (msg->msg_type != M_MOUSE_LEFT_CLICK || dialog_input == -1)
		return;
	if (selected_index == -1 || selected_index >= total_items)
		return;

	char newName[MAX_SHORT_STRLEN];
	strcpy(newName, desktop.widgets[dialog_input].context.inputfield->text);

	if (strlen(newName) == 0 || strcmp(newName, selected_name) == 0) {
		clearDialog();
		return;
	}

	char oldPath[MAX_LONG_STRLEN], newPath[MAX_LONG_STRLEN];
	int pathLen = strlen(current_path);

	if (pathLen > 0) {
		if (pathLen + strlen(selected_name) + 2 >= MAX_LONG_STRLEN ||
		    pathLen + strlen(newName) + 2 >= MAX_LONG_STRLEN) {
			clearDialog();
			return;
		}
		strcpy(oldPath, current_path);
		strcat(oldPath, "/");
		strcat(oldPath, selected_name);
		strcpy(newPath, current_path);
		strcat(newPath, "/");
		strcat(newPath, newName);
	} else {
		if (strlen(selected_name) >= MAX_LONG_STRLEN ||
		    strlen(newName) >= MAX_LONG_STRLEN) {
			clearDialog();
			return;
		}
		strcpy(oldPath, selected_name);
		strcpy(newPath, newName);
	}

	if (link(oldPath, newPath) == 0) {
		unlink(oldPath);
	}

	clearDialog();
	need_refresh = 1;
}

void dialogInputHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_KEY_DOWN)
		return;

	inputFieldKeyHandler(w, msg);
}

void showDialog(char *title, char *defaultText,
		void (*onConfirm)(Widget *, message *)) {
	clearDialog();
	dialog_active = 1;

	int dialogW = 340;
	int dialogH = 140;
	int dialogX = (desktop.width - dialogW) / 2;
	int dialogY = (desktop.height - dialogH) / 2;

	dialog_bg = addColorFillWidget(&desktop, color_dialog_overlay, 0, 0,
				       desktop.width, desktop.height, 0,
				       emptyHandler);
	dialog_panel =
		addColorFillWidget(&desktop, color_dialog_bg, dialogX, dialogY,
				   dialogW, dialogH, 0, emptyHandler);

	dialog_title =
		addTextWidget(&desktop, color_text_dark, title, dialogX + 20,
			      dialogY + 20, dialogW - 40, 20, 0, emptyHandler);

	dialog_input = addInputFieldWidget(
		&desktop, color_text_dark, defaultText, dialogX + 20,
		dialogY + 55, dialogW - 40, 28, 0, dialogInputHandler);

	dialog_btn1 = addButtonWidget(&desktop, color_white, color_button, "OK",
				      dialogX + dialogW - 160, dialogY + 100,
				      70, 28, 0, onConfirm);

	dialog_btn2 = addButtonWidget(&desktop, color_text_dark, color_border,
				      "Cancel", dialogX + dialogW - 80,
				      dialogY + 100, 70, 28, 0, cancelDialog);

	desktop.keyfocus = dialog_input;
	desktop.needsRepaint = 1;
}

void handleBack(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (dialog_active)
			return;

		char *parentPath = getParentPath(current_path);
		if (strcmp(parentPath, "/") == 0) {
			strcpy(current_path, "");
		} else {
			strcpy(current_path, parentPath);
		}
		need_refresh = 1;
	}
}

void handleHome(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (dialog_active)
			return;
		strcpy(current_path, "");
		need_refresh = 1;
	}
}

void handleNewFolder(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (dialog_active)
			return;
		showDialog("New Folder", "newfolder", confirmNewFolder);
	}
}

void handleNewFile(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (dialog_active)
			return;
		showDialog("New File", "newfile.txt", confirmNewFile);
	}
}

void handleRename(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (dialog_active)
			return;
		if (selected_index == -1 || selected_index >= total_items)
			return;
		if (isProtected(selected_name))
			return;
		showDialog("Rename", selected_name, confirmRename);
	}
}

void handleDelete(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (dialog_active)
			return;
		if (selected_index == -1 || selected_index >= total_items)
			return;
		if (isProtected(selected_name))
			return;

		char fullPath[MAX_LONG_STRLEN];
		int pathLen = strlen(current_path);

		if (pathLen > 0) {
			if (pathLen + strlen(selected_name) + 2 >=
			    MAX_LONG_STRLEN)
				return;
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, selected_name);
		} else {
			if (strlen(selected_name) >= MAX_LONG_STRLEN)
				return;
			strcpy(fullPath, selected_name);
		}

		unlink(fullPath);
		need_refresh = 1;
	}
}

void handleFileClick(Widget *w, message *msg) {
	if (dialog_active)
		return;

	int idx = -1;
	int wid = findWidgetId(&desktop, w);

	for (int i = 0; i < total_items; i++) {
		if (items[i].widget_id == wid) {
			idx = i;
			break;
		}
	}

	if (idx == -1 || idx >= total_items)
		return;

	if (msg->msg_type == M_MOUSE_DBCLICK) {
		if (items[idx].is_dir) {
			int current_path_length = strlen(current_path);
			int name_length = strlen(items[idx].name);

			if (current_path_length + name_length + 2 >=
			    MAX_LONG_STRLEN)
				return;

			if (current_path_length > 0) {
				current_path[current_path_length] = '/';
				strcpy(current_path + current_path_length + 1,
				       items[idx].name);
			} else {
				strcpy(current_path, items[idx].name);
			}
			need_refresh = 1;
		} else {
			char fullPath[MAX_LONG_STRLEN];
			int pathLen = strlen(current_path);
			int nameLen = strlen(items[idx].name);

			if (pathLen > 0) {
				if (pathLen + nameLen + 2 >= MAX_LONG_STRLEN)
					return;
				strcpy(fullPath, current_path);
				strcat(fullPath, "/");
				strcat(fullPath, items[idx].name);
			} else {
				if (nameLen >= MAX_LONG_STRLEN)
					return;
				strcpy(fullPath, items[idx].name);
			}

			if (fork() == 0) {
				if (canOpenWithEditor(items[idx].name)) {
					char *argv2[] = {"editor", fullPath, 0};
					exec(argv2[0], argv2);
				} else {
					char *argv2[] = {items[idx].name, 0};
					exec(argv2[0], argv2);
				}
				exit();
			}
		}
	} else if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		selected_index = idx;

		int nameLen = strlen(items[idx].name);
		if (nameLen >= MAX_SHORT_STRLEN)
			nameLen = MAX_SHORT_STRLEN - 1;

		memmove(selected_name, items[idx].name, nameLen);
		selected_name[nameLen] = '\0';

		is_selected_dir = items[idx].is_dir;
		desktop.needsRepaint = 1;
	}
}

void handleKeyboard(Widget *w, message *msg) {
	if (msg->msg_type != M_KEY_DOWN)
		return;
	if (dialog_active)
		return;

	int key = msg->params[0];

	if (key == KEY_DN) {
		if (selected_index < total_items - 1) {
			selected_index++;
			if (selected_index >= 0 &&
			    selected_index < total_items) {
				strcpy(selected_name,
				       items[selected_index].name);
				is_selected_dir = items[selected_index].is_dir;
				desktop.needsRepaint = 1;
			}
		} else if (selected_index == -1 && total_items > 0) {
			selected_index = 0;
			strcpy(selected_name, items[0].name);
			is_selected_dir = items[0].is_dir;
			desktop.needsRepaint = 1;
		}
	} else if (key == KEY_UP) {
		if (selected_index > 0) {
			selected_index--;
			if (selected_index >= 0 &&
			    selected_index < total_items) {
				strcpy(selected_name,
				       items[selected_index].name);
				is_selected_dir = items[selected_index].is_dir;
				desktop.needsRepaint = 1;
			}
		} else if (selected_index == -1 && total_items > 0) {
			selected_index = 0;
			strcpy(selected_name, items[0].name);
			is_selected_dir = items[0].is_dir;
			desktop.needsRepaint = 1;
		}
	} else if (key == '\n' && selected_index >= 0 &&
		   selected_index < total_items) {
		if (is_selected_dir) {
			int current_path_length = strlen(current_path);
			int name_length = strlen(selected_name);

			if (current_path_length + name_length + 2 <
			    MAX_LONG_STRLEN) {
				if (current_path_length > 0) {
					current_path[current_path_length] = '/';
					strcpy(current_path +
						       current_path_length + 1,
					       selected_name);
				} else {
					strcpy(current_path, selected_name);
				}
				need_refresh = 1;
			}
		} else {
			char fullPath[MAX_LONG_STRLEN];
			int pathLen = strlen(current_path);
			int nameLen = strlen(selected_name);

			if (pathLen > 0) {
				if (pathLen + nameLen + 2 < MAX_LONG_STRLEN) {
					strcpy(fullPath, current_path);
					strcat(fullPath, "/");
					strcat(fullPath, selected_name);
				} else {
					return;
				}
			} else {
				if (nameLen < MAX_LONG_STRLEN) {
					strcpy(fullPath, selected_name);
				} else {
					return;
				}
			}

			if (fork() == 0) {
				if (canOpenWithEditor(selected_name)) {
					char *argv2[] = {"editor", fullPath, 0};
					exec(argv2[0], argv2);
				} else {
					char *argv2[] = {selected_name, 0};
					exec(argv2[0], argv2);
				}
				exit();
			}
		}
	}
}

void removeAllScrollableWidgets() {
	int toRemove[MAX_WIDGET_SIZE];
	int removeCount = 0;

	for (int p = desktop.widgetlisthead; p != -1;
	     p = desktop.widgets[p].next) {
		if (p >= 0 && p < MAX_WIDGET_SIZE &&
		    desktop.widgets[p].scrollable == 1) {
			toRemove[removeCount++] = p;
			if (removeCount >= MAX_WIDGET_SIZE)
				break;
		}
	}

	for (int i = 0; i < removeCount; i++) {
		removeWidget(&desktop, toRemove[i]);
	}
}

void loadFiles() {
	removeAllScrollableWidgets();

	char pathBuf[MAX_LONG_STRLEN];
	char *p;
	int fd;
	struct dirent de;
	struct stat st;

	char openPath[MAX_LONG_STRLEN];
	if (strlen(current_path) > 0) {
		if (strlen(current_path) >= MAX_LONG_STRLEN)
			return;
		strcpy(openPath, current_path);
	} else {
		strcpy(openPath, ".");
	}

	if ((fd = open(openPath, 0)) < 0)
		return;
	if (fstat(fd, &st) < 0) {
		close(fd);
		return;
	}

	if (st.type != T_DIR) {
		close(fd);
		return;
	}

	if (strlen(current_path) + 1 + DIRSIZ + 1 > sizeof(pathBuf)) {
		close(fd);
		return;
	}

	strcpy(pathBuf, current_path);
	p = pathBuf + strlen(pathBuf);
	if (strlen(current_path) > 0)
		*p++ = '/';

	int contentY = TOPBAR_HEIGHT + CONTENT_PADDING;
	int contentX = CONTENT_PADDING;

	total_items = 0;

	while (read(fd, &de, sizeof(de)) == sizeof(de)) {
		if (de.inum == 0)
			continue;

		memmove(p, de.name, DIRSIZ);
		p[DIRSIZ] = 0;

		if (stat(pathBuf, &st) < 0)
			continue;

		char formatName[MAX_SHORT_STRLEN];
		char *fname = fmtname(pathBuf);
		if (!fname)
			continue;

		int nameLen = strlen(fname);
		if (nameLen >= MAX_SHORT_STRLEN)
			nameLen = MAX_SHORT_STRLEN - 1;

		memmove(formatName, fname, nameLen);
		formatName[nameLen] = '\0';

		int shouldShow = 0;
		int isDir = 0;

		if (st.type == T_DIR && strcmp(formatName, ".") != 0 &&
		    strcmp(formatName, "..") != 0) {
			shouldShow = 1;
			isDir = 1;
		} else if (st.type == T_FILE && shouldShowFile(formatName)) {
			shouldShow = 1;
			isDir = 0;
		}

		if (!shouldShow)
			continue;

		int y = contentY + total_items * (ITEM_HEIGHT + ITEM_PADDING);

		char displayText[MAX_SHORT_STRLEN + 10];
		if (isDir) {
			strcpy(displayText, "[DIR]  ");
			if (strlen(displayText) + strlen(formatName) <
			    MAX_SHORT_STRLEN + 10) {
				strcat(displayText, formatName);
			}
		} else {
			strcpy(displayText, "[FILE] ");
			if (strlen(displayText) + strlen(formatName) <
			    MAX_SHORT_STRLEN + 10) {
				strcat(displayText, formatName);
			}
		}

		struct RGBA itemColor = isDir ? color_folder : color_file;

		int wid =
			addTextWidget(&desktop, itemColor, displayText,
				      contentX, y, desktop.width - contentX * 2,
				      ITEM_HEIGHT, 1, handleFileClick);

		if (total_items < MAX_WIDGET_SIZE) {
			nameLen = strlen(formatName);
			if (nameLen >= MAX_SHORT_STRLEN)
				nameLen = MAX_SHORT_STRLEN - 1;

			memmove(items[total_items].name, formatName, nameLen);
			items[total_items].name[nameLen] = '\0';

			items[total_items].is_dir = isDir;
			items[total_items].widget_id = wid;
			items[total_items].y_pos = y;
			total_items++;
		}

		if (total_items >= MAX_WIDGET_SIZE - 10)
			break;
	}

	close(fd);

	char pathText[MAX_LONG_STRLEN];
	if (strlen(current_path) > 0) {
		int len = strlen(current_path);
		if (len >= MAX_LONG_STRLEN)
			len = MAX_LONG_STRLEN - 1;
		memmove(pathText, current_path, len);
		pathText[len] = '\0';
	} else {
		strcpy(pathText, "/");
	}

	if (path_display_widget >= 0 && path_display_widget < MAX_WIDGET_SIZE) {
		strcpy(desktop.widgets[path_display_widget].context.text->text,
		       pathText);
	}

	selected_index = -1;
	desktop.needsRepaint = 1;
}

int main(int argc, char *argv[]) {
	desktop.width = 680;
	desktop.height = 480;
	desktop.hasTitleBar = 1;
	createWindow(&desktop, "File Explorer");

	initColors();

	addColorFillWidget(&desktop, color_bg, 0, 0, desktop.width,
			   desktop.height, 0, emptyHandler);

	topbar_widget =
		addColorFillWidget(&desktop, color_topbar, 0, 0, desktop.width,
				   TOPBAR_HEIGHT, 0, emptyHandler);

	addButtonWidget(&desktop, color_white, color_button, "<", 8, 8, 32, 24,
			0, handleBack);
	addButtonWidget(&desktop, color_white, color_button, "Home", 48, 8, 50,
			24, 0, handleHome);
	addButtonWidget(&desktop, color_white, color_button, "+Dir", 106, 8, 48,
			24, 0, handleNewFolder);
	addButtonWidget(&desktop, color_white, color_button, "+File", 162, 8,
			48, 24, 0, handleNewFile);
	addButtonWidget(&desktop, color_white, color_button, "Rename", 218, 8,
			60, 24, 0, handleRename);
	addButtonWidget(&desktop, color_white, color_button, "Delete", 286, 8,
			58, 24, 0, handleDelete);

	path_display_widget =
		addTextWidget(&desktop, color_text_light, "/", 354, 12,
			      desktop.width - 364, 18, 0, emptyHandler);

	statusbar_widget = addColorFillWidget(
		&desktop, color_statusbar, 0, desktop.height - STATUSBAR_HEIGHT,
		desktop.width, STATUSBAR_HEIGHT, 0, emptyHandler);

	memset(current_path, 0, MAX_LONG_STRLEN);
	memset(selected_name, 0, MAX_SHORT_STRLEN);

	loadFiles();

	addColorFillWidget(&desktop, color_bg, 0, 0, 0, 0, 0, handleKeyboard);
	desktop.keyfocus = desktop.widgetlisttail;

	while (1) {
		updateWindow(&desktop);

		if (need_refresh) {
			need_refresh = 0;
			loadFiles();
		}

		if (!dialog_active && selected_index >= 0 &&
		    selected_index < total_items) {
			int y = items[selected_index].y_pos -
				desktop.scrollOffsetY;

			int contentTop = TOPBAR_HEIGHT;
			int contentBottom = desktop.height - STATUSBAR_HEIGHT;

			if (y + ITEM_HEIGHT > contentTop && y < contentBottom) {
				drawFillRect(&desktop, color_selected,
					     CONTENT_PADDING, y,
					     desktop.width -
						     CONTENT_PADDING * 2,
					     ITEM_HEIGHT);

				RGB borderAccent;
				borderAccent.R = color_accent.R;
				borderAccent.G = color_accent.G;
				borderAccent.B = color_accent.B;
				drawRect(&desktop, borderAccent,
					 CONTENT_PADDING, y,
					 desktop.width - CONTENT_PADDING * 2,
					 ITEM_HEIGHT);
			}
		}
	}

	return 0;
}