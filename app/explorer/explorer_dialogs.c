#include "explorer.h"
#include "user.h"
#include "fcntl.h"

void clearDialog() {
	if (state.dialog_bg != -1) {
		removeWidget(&state.desktop, state.dialog_bg);
		state.dialog_bg = -1;
	}
	if (state.dialog_panel != -1) {
		removeWidget(&state.desktop, state.dialog_panel);
		state.dialog_panel = -1;
	}
	if (state.dialog_title != -1) {
		removeWidget(&state.desktop, state.dialog_title);
		state.dialog_title = -1;
	}
	if (state.dialog_input != -1) {
		removeWidget(&state.desktop, state.dialog_input);
		state.dialog_input = -1;
	}
	if (state.dialog_btn1 != -1) {
		removeWidget(&state.desktop, state.dialog_btn1);
		state.dialog_btn1 = -1;
	}
	if (state.dialog_btn2 != -1) {
		removeWidget(&state.desktop, state.dialog_btn2);
		state.dialog_btn2 = -1;
	}
	state.dialog_active = 0;
	state.desktop.needsRepaint = 1;
}

void cancelDialog(Widget *w, message *msg) {
	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearDialog();
	}
}

void refreshDialogInput() {
	if (state.dialog_input == -1)
		return;

	// Ambil posisi dan ukuran input field
	Widget *inputWidget = &state.desktop.widgets[state.dialog_input];
	int x = inputWidget->position.xmin;
	int y = inputWidget->position.ymin;
	int w = inputWidget->position.xmax - inputWidget->position.xmin;
	int h = inputWidget->position.ymax - inputWidget->position.ymin;

	// Clear area input field dengan background putih
	RGBA white_bg;
	white_bg.R = 255;
	white_bg.G = 255;
	white_bg.B = 255;
	white_bg.A = 255;

	drawFillRect(&state.desktop, white_bg, x, y, w, h);

	// Gambar border input field
	RGB border;
	border.R = state.colors.color_input_border.R;
	border.G = state.colors.color_input_border.G;
	border.B = state.colors.color_input_border.B;

	drawRect(&state.desktop, border, x, y, w, h);

	// Gambar teks yang ada di input field
	drawString(&state.desktop, inputWidget->context.inputfield->text,
		   state.colors.color_text_dark, x + 4, y + 6, w - 8, h - 12);

	// Gambar cursor
	int cursor_pos = inputWidget->context.inputfield->current_pos;
	int cursor_x = x + 4;
	int cursor_y = y + 6;

	// Hitung posisi cursor berdasarkan current_pos
	for (int i = 0; i < cursor_pos && inputWidget->context.inputfield->text[i] != '\0'; i++) {
		cursor_x += 8; // CHARACTER_WIDTH = 8
		if (cursor_x > x + w - 8) {
			cursor_x = x + 4;
			cursor_y += 16; // CHARACTER_HEIGHT = 16
		}
	}

	// Gambar garis cursor
	RGBA cursor_color;
	cursor_color.R = state.colors.color_text_dark.R;
	cursor_color.G = state.colors.color_text_dark.G;
	cursor_color.B = state.colors.color_text_dark.B;
	cursor_color.A = 255;

	drawFillRect(&state.desktop, cursor_color, cursor_x, cursor_y, 2, 14);
}

void dialogInputHandler(Widget *w, message *msg) {
	if (msg->msg_type != M_KEY_DOWN)
		return;

	inputFieldKeyHandler(w, msg);
	
	// Refresh input field untuk menghilangkan glitch
	refreshDialogInput();
	
	state.desktop.needsRepaint = 1;
}

void showDialog(char *title, char *defaultText, void (*onConfirm)(Widget *, message *)) {
	clearDialog();
	state.dialog_active = 1;

	int dialogW = 340;
	int dialogH = 140;
	int dialogX = (state.desktop.width - dialogW) / 2;
	int dialogY = (state.desktop.height - dialogH) / 2;

	// Background overlay semi-transparan
	state.dialog_bg = addColorFillWidget(&state.desktop, state.colors.color_dialog_overlay, 0, 0,
					     state.desktop.width, state.desktop.height, 0, emptyHandler);

	// Panel dialog putih
	state.dialog_panel = addColorFillWidget(&state.desktop, state.colors.color_dialog_bg, dialogX, dialogY,
						dialogW, dialogH, 0, emptyHandler);

	// Title
	state.dialog_title = addTextWidget(&state.desktop, state.colors.color_text_dark, title, dialogX + 20,
					   dialogY + 20, dialogW - 40, 20, 0, emptyHandler);

	// Input field
	state.dialog_input = addInputFieldWidget(&state.desktop, state.colors.color_text_dark, defaultText,
						 dialogX + 20, dialogY + 55, dialogW - 40, 28, 0, dialogInputHandler);

	// Set cursor position ke akhir teks default
	if (state.dialog_input >= 0 && state.dialog_input < MAX_WIDGET_SIZE) {
		state.desktop.widgets[state.dialog_input].context.inputfield->current_pos = strlen(defaultText);
	}

	// Tombol OK
	state.dialog_btn1 = addButtonWidget(&state.desktop, state.colors.color_white, state.colors.color_button, "OK",
					    dialogX + dialogW - 160, dialogY + 100, 70, 28, 0, onConfirm);

	// Tombol Cancel
	state.dialog_btn2 = addButtonWidget(&state.desktop, state.colors.color_text_dark, state.colors.color_border,
					    "Cancel", dialogX + dialogW - 80, dialogY + 100, 70, 28, 0, cancelDialog);

	state.desktop.keyfocus = state.dialog_input;
	state.desktop.needsRepaint = 1;
}