#include "explorer.h"
#include "user.h"

int main(int argc, char *argv[]) {
	state.desktop.width = 680;
	state.desktop.height = 480;
	state.desktop.hasTitleBar = 1;
	createWindow(&state.desktop, "File Explorer");

	initColors();

	memset(state.current_path, 0, MAX_LONG_STRLEN);
	memset(state.selected_name, 0, MAX_SHORT_STRLEN);
	
	state.selected_index = -1;
	state.total_items = 0;
	state.is_selected_dir = 0;
	state.dialog_active = 0;
	state.dialog_bg = -1;
	state.dialog_panel = -1;
	state.dialog_input = -1;
	state.dialog_btn1 = -1;
	state.dialog_btn2 = -1;
	state.dialog_title = -1;
	state.need_refresh = 0;

	initUI();
	loadFiles();

	while (1) {
		updateWindow(&state.desktop);

		if (state.need_refresh) {
			state.need_refresh = 0;
			loadFiles();
		}

		if (!state.dialog_active && state.selected_index >= 0 && state.selected_index < state.total_items) {
			int y = state.items[state.selected_index].y_pos - state.desktop.scrollOffsetY;

			int contentTop = TOPBAR_HEIGHT;
			int contentBottom = state.desktop.height - STATUSBAR_HEIGHT;

			if (y + ITEM_HEIGHT > contentTop && y < contentBottom) {
				drawFillRect(&state.desktop, state.colors.color_selected, CONTENT_PADDING, y,
					     state.desktop.width - CONTENT_PADDING * 2, ITEM_HEIGHT);

				RGB borderAccent;
				borderAccent.R = state.colors.color_accent.R;
				borderAccent.G = state.colors.color_accent.G;
				borderAccent.B = state.colors.color_accent.B;
				drawRect(&state.desktop, borderAccent, CONTENT_PADDING, y,
					 state.desktop.width - CONTENT_PADDING * 2, ITEM_HEIGHT);
			}
		}
	}

	return 0;
}