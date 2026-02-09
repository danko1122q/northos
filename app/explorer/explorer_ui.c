#include "explorer.h"
#include "user.h"
#include "fcntl.h"
#include "fs.h"

void removeAllScrollableWidgets() {
	int toRemove[MAX_WIDGET_SIZE];
	int removeCount = 0;

	for (int p = state.desktop.widgetlisthead; p != -1; p = state.desktop.widgets[p].next) {
		if (p >= 0 && p < MAX_WIDGET_SIZE && state.desktop.widgets[p].scrollable == 1) {
			toRemove[removeCount++] = p;
			if (removeCount >= MAX_WIDGET_SIZE)
				break;
		}
	}

	for (int i = 0; i < removeCount; i++) {
		removeWidget(&state.desktop, toRemove[i]);
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
	if (strlen(state.current_path) > 0) {
		if (strlen(state.current_path) >= MAX_LONG_STRLEN)
			return;
		strcpy(openPath, state.current_path);
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

	if (strlen(state.current_path) + 1 + DIRSIZ + 1 > sizeof(pathBuf)) {
		close(fd);
		return;
	}

	strcpy(pathBuf, state.current_path);
	p = pathBuf + strlen(pathBuf);
	if (strlen(state.current_path) > 0)
		*p++ = '/';

	int contentY = TOPBAR_HEIGHT + CONTENT_PADDING;
	int contentX = CONTENT_PADDING;

	state.total_items = 0;

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
		int isExec = 0;

		if (st.type == T_DIR && strcmp(formatName, ".") != 0 && strcmp(formatName, "..") != 0) {
			shouldShow = 1;
			isDir = 1;
		} else if (st.type == T_FILE && shouldShowFile(formatName)) {
			shouldShow = 1;
			isDir = 0;
			isExec = isExecutable(formatName);
		}

		if (!shouldShow)
			continue;

		int y = contentY + state.total_items * (ITEM_HEIGHT + ITEM_PADDING);

		char displayText[MAX_SHORT_STRLEN + 10];
		RGBA itemColor;

		if (isDir) {
			strcpy(displayText, "[DIR]  ");
			if (strlen(displayText) + strlen(formatName) < MAX_SHORT_STRLEN + 10) {
				strcat(displayText, formatName);
			}
			itemColor = state.colors.color_folder;
		} else if (isExec) {
			strcpy(displayText, "[EXEC] ");
			if (strlen(displayText) + strlen(formatName) < MAX_SHORT_STRLEN + 10) {
				strcat(displayText, formatName);
			}
			// Warna hijau untuk executable
			itemColor.R = 40;
			itemColor.G = 167;
			itemColor.B = 69;
			itemColor.A = 255;
		} else {
			strcpy(displayText, "[FILE] ");
			if (strlen(displayText) + strlen(formatName) < MAX_SHORT_STRLEN + 10) {
				strcat(displayText, formatName);
			}
			itemColor = state.colors.color_file;
		}

		int wid = addTextWidget(&state.desktop, itemColor, displayText, contentX, y,
					state.desktop.width - contentX * 2, ITEM_HEIGHT, 1, handleFileClick);

		if (state.total_items < MAX_WIDGET_SIZE) {
			nameLen = strlen(formatName);
			if (nameLen >= MAX_SHORT_STRLEN)
				nameLen = MAX_SHORT_STRLEN - 1;

			memmove(state.items[state.total_items].name, formatName, nameLen);
			state.items[state.total_items].name[nameLen] = '\0';

			state.items[state.total_items].is_dir = isDir;
			state.items[state.total_items].widget_id = wid;
			state.items[state.total_items].y_pos = y;
			state.total_items++;
		}

		if (state.total_items >= MAX_WIDGET_SIZE - 10)
			break;
	}

	close(fd);

	char pathText[MAX_LONG_STRLEN];
	if (strlen(state.current_path) > 0) {
		int len = strlen(state.current_path);
		if (len >= MAX_LONG_STRLEN)
			len = MAX_LONG_STRLEN - 1;
		memmove(pathText, state.current_path, len);
		pathText[len] = '\0';
	} else {
		strcpy(pathText, "/");
	}

	if (state.path_display_widget >= 0 && state.path_display_widget < MAX_WIDGET_SIZE) {
		strcpy(state.desktop.widgets[state.path_display_widget].context.text->text, pathText);
	}

	state.selected_index = -1;
	state.desktop.needsRepaint = 1;
}

void initUI() {
	addColorFillWidget(&state.desktop, state.colors.color_bg, 0, 0, state.desktop.width,
			   state.desktop.height, 0, emptyHandler);

	state.topbar_widget = addColorFillWidget(&state.desktop, state.colors.color_topbar, 0, 0,
						 state.desktop.width, TOPBAR_HEIGHT, 0, emptyHandler);

	addButtonWidget(&state.desktop, state.colors.color_white, state.colors.color_button, "<", 8, 8, 32, 24, 0, handleBack);
	addButtonWidget(&state.desktop, state.colors.color_white, state.colors.color_button, "Home", 48, 8, 50, 24, 0, handleHome);
	addButtonWidget(&state.desktop, state.colors.color_white, state.colors.color_button, "+Dir", 106, 8, 48, 24, 0, handleNewFolder);
	addButtonWidget(&state.desktop, state.colors.color_white, state.colors.color_button, "+File", 162, 8, 48, 24, 0, handleNewFile);
	addButtonWidget(&state.desktop, state.colors.color_white, state.colors.color_button, "Rename", 218, 8, 60, 24, 0, handleRename);
	addButtonWidget(&state.desktop, state.colors.color_white, state.colors.color_button, "Delete", 286, 8, 58, 24, 0, handleDelete);

	state.path_display_widget = addTextWidget(&state.desktop, state.colors.color_text_light, "/", 354, 12,
						  state.desktop.width - 364, 18, 0, emptyHandler);

	state.statusbar_widget = addColorFillWidget(&state.desktop, state.colors.color_statusbar, 0,
						    state.desktop.height - STATUSBAR_HEIGHT,
						    state.desktop.width, STATUSBAR_HEIGHT, 0, emptyHandler);

	addColorFillWidget(&state.desktop, state.colors.color_bg, 0, 0, 0, 0, 0, handleKeyboard);
	state.desktop.keyfocus = state.desktop.widgetlisttail;
}