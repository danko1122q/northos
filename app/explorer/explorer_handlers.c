#include "explorer.h"
#include "user.h"

void handleBack(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (state.dialog_active)
			return;

		char *parentPath = getParentPath(state.current_path);
		if (strcmp(parentPath, "/") == 0) {
			strcpy(state.current_path, "");
		} else {
			strcpy(state.current_path, parentPath);
		}
		state.need_refresh = 1;
	}
}

void handleHome(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (state.dialog_active)
			return;
		strcpy(state.current_path, "");
		state.need_refresh = 1;
	}
}

void handleNewFolder(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (state.dialog_active)
			return;
		showDialog("New Folder", "newfolder", confirmNewFolder);
	}
}

void handleNewFile(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (state.dialog_active)
			return;
		showDialog("New File", "newfile.txt", confirmNewFile);
	}
}

void handleRename(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (state.dialog_active)
			return;
		if (state.selected_index == -1 || state.selected_index >= state.total_items)
			return;
		if (isProtected(state.selected_name))
			return;
		showDialog("Rename", state.selected_name, confirmRename);
	}
}

void handleDelete(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (state.dialog_active)
			return;
		if (state.selected_index == -1 || state.selected_index >= state.total_items)
			return;
		if (isProtected(state.selected_name))
			return;

		char fullPath[MAX_LONG_STRLEN];
		int pathLen = strlen(state.current_path);

		if (pathLen > 0) {
			if (pathLen + strlen(state.selected_name) + 2 >= MAX_LONG_STRLEN)
				return;
			strcpy(fullPath, state.current_path);
			strcat(fullPath, "/");
			strcat(fullPath, state.selected_name);
		} else {
			if (strlen(state.selected_name) >= MAX_LONG_STRLEN)
				return;
			strcpy(fullPath, state.selected_name);
		}

		unlink(fullPath);
		state.need_refresh = 1;
	}
}

void handleFileClick(Widget *w, message *msg) {
	if (state.dialog_active)
		return;

	int idx = -1;
	int wid = findWidgetId(&state.desktop, w);

	for (int i = 0; i < state.total_items; i++) {
		if (state.items[i].widget_id == wid) {
			idx = i;
			break;
		}
	}

	if (idx == -1 || idx >= state.total_items)
		return;

	if (msg->msg_type == M_MOUSE_DBCLICK) {
		if (state.items[idx].is_dir) {
			int current_path_length = strlen(state.current_path);
			int name_length = strlen(state.items[idx].name);

			if (current_path_length + name_length + 2 >= MAX_LONG_STRLEN)
				return;

			if (current_path_length > 0) {
				state.current_path[current_path_length] = '/';
				strcpy(state.current_path + current_path_length + 1, state.items[idx].name);
			} else {
				strcpy(state.current_path, state.items[idx].name);
			}
			state.need_refresh = 1;
		} else {
			char fullPath[MAX_LONG_STRLEN];
			int pathLen = strlen(state.current_path);
			int nameLen = strlen(state.items[idx].name);

			if (pathLen > 0) {
				if (pathLen + nameLen + 2 >= MAX_LONG_STRLEN)
					return;
				strcpy(fullPath, state.current_path);
				strcat(fullPath, "/");
				strcat(fullPath, state.items[idx].name);
			} else {
				if (nameLen >= MAX_LONG_STRLEN)
					return;
				strcpy(fullPath, state.items[idx].name);
			}

			if (fork() == 0) {
				if (canOpenWithEditor(state.items[idx].name)) {
					char *argv2[] = {"editor", fullPath, 0};
					exec(argv2[0], argv2);
				} else {
					char *argv2[] = {state.items[idx].name, 0};
					exec(argv2[0], argv2);
				}
				exit();
			}
		}
	} else if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		state.selected_index = idx;

		int nameLen = strlen(state.items[idx].name);
		if (nameLen >= MAX_SHORT_STRLEN)
			nameLen = MAX_SHORT_STRLEN - 1;

		memmove(state.selected_name, state.items[idx].name, nameLen);
		state.selected_name[nameLen] = '\0';

		state.is_selected_dir = state.items[idx].is_dir;
		state.desktop.needsRepaint = 1;
	}
}

void handleKeyboard(Widget *w, message *msg) {
	if (msg->msg_type != M_KEY_DOWN)
		return;
	if (state.dialog_active)
		return;

	int key = msg->params[0];

	if (key == KEY_DN) {
		if (state.selected_index < state.total_items - 1) {
			state.selected_index++;
			if (state.selected_index >= 0 && state.selected_index < state.total_items) {
				strcpy(state.selected_name, state.items[state.selected_index].name);
				state.is_selected_dir = state.items[state.selected_index].is_dir;
				state.desktop.needsRepaint = 1;
			}
		} else if (state.selected_index == -1 && state.total_items > 0) {
			state.selected_index = 0;
			strcpy(state.selected_name, state.items[0].name);
			state.is_selected_dir = state.items[0].is_dir;
			state.desktop.needsRepaint = 1;
		}
	} else if (key == KEY_UP) {
		if (state.selected_index > 0) {
			state.selected_index--;
			if (state.selected_index >= 0 && state.selected_index < state.total_items) {
				strcpy(state.selected_name, state.items[state.selected_index].name);
				state.is_selected_dir = state.items[state.selected_index].is_dir;
				state.desktop.needsRepaint = 1;
			}
		} else if (state.selected_index == -1 && state.total_items > 0) {
			state.selected_index = 0;
			strcpy(state.selected_name, state.items[0].name);
			state.is_selected_dir = state.items[0].is_dir;
			state.desktop.needsRepaint = 1;
		}
	} else if (key == '\n' && state.selected_index >= 0 && state.selected_index < state.total_items) {
		if (state.is_selected_dir) {
			int current_path_length = strlen(state.current_path);
			int name_length = strlen(state.selected_name);

			if (current_path_length + name_length + 2 < MAX_LONG_STRLEN) {
				if (current_path_length > 0) {
					state.current_path[current_path_length] = '/';
					strcpy(state.current_path + current_path_length + 1, state.selected_name);
				} else {
					strcpy(state.current_path, state.selected_name);
				}
				state.need_refresh = 1;
			}
		} else {
			char fullPath[MAX_LONG_STRLEN];
			int pathLen = strlen(state.current_path);
			int nameLen = strlen(state.selected_name);

			if (pathLen > 0) {
				if (pathLen + nameLen + 2 < MAX_LONG_STRLEN) {
					strcpy(fullPath, state.current_path);
					strcat(fullPath, "/");
					strcat(fullPath, state.selected_name);
				} else {
					return;
				}
			} else {
				if (nameLen < MAX_LONG_STRLEN) {
					strcpy(fullPath, state.selected_name);
				} else {
					return;
				}
			}

			if (fork() == 0) {
				if (canOpenWithEditor(state.selected_name)) {
					char *argv2[] = {"editor", fullPath, 0};
					exec(argv2[0], argv2);
				} else {
					char *argv2[] = {state.selected_name, 0};
					exec(argv2[0], argv2);
				}
				exit();
			}
		}
	}
}