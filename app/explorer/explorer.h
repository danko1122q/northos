#ifndef EXPLORER_H
#define EXPLORER_H

#include "gui.h"
#include "user_window.h"
#include "stat.h"

#define TOPBAR_HEIGHT 40
#define STATUSBAR_HEIGHT 24
#define ITEM_HEIGHT 24
#define ITEM_PADDING 4
#define CONTENT_PADDING 8
#define MAX_PATH_DEPTH 256

typedef struct {
	char name[MAX_SHORT_STRLEN];
	int is_dir;
	int widget_id;
	int y_pos;
} FileItem;

typedef struct {
	RGBA color_bg;
	RGBA color_topbar;
	RGBA color_statusbar;
	RGBA color_text_dark;
	RGBA color_text_light;
	RGBA color_accent;
	RGBA color_folder;
	RGBA color_file;
	RGBA color_selected;
	RGBA color_border;
	RGBA color_dialog_bg;
	RGBA color_dialog_overlay;
	RGBA color_button;
	RGBA color_white;
	RGBA color_input_border;
} ColorScheme;

typedef struct {
	window desktop;
	char current_path[MAX_LONG_STRLEN];
	int topbar_widget;
	int statusbar_widget;
	int path_display_widget;
	int selected_index;
	int total_items;
	char selected_name[MAX_SHORT_STRLEN];
	int is_selected_dir;
	int dialog_active;
	int dialog_bg;
	int dialog_panel;
	int dialog_input;
	int dialog_btn1;
	int dialog_btn2;
	int dialog_title;
	int need_refresh;
	FileItem items[MAX_WIDGET_SIZE];
	ColorScheme colors;
} ExplorerState;

extern ExplorerState state;

// Utility functions
void initColors(void);
char *getFileExtension(char *filename);
int shouldShowFile(char *filename);
int canOpenWithEditor(char *filename);
int isExecutable(char *filename);
int isProtected(char *name);
char *getParentPath(char *path);
char *fmtname(char *path);

// Dialog functions
void clearDialog(void);
void showDialog(char *title, char *defaultText, void (*onConfirm)(Widget *, message *));
void refreshDialogInput(void);

// Dialog callbacks
void confirmNewFolder(Widget *w, message *msg);
void confirmNewFile(Widget *w, message *msg);
void confirmRename(Widget *w, message *msg);
void cancelDialog(Widget *w, message *msg);
void dialogInputHandler(Widget *w, message *msg);

// Event handlers
void handleBack(Widget *w, message *msg);
void handleHome(Widget *w, message *msg);
void handleNewFolder(Widget *w, message *msg);
void handleNewFile(Widget *w, message *msg);
void handleRename(Widget *w, message *msg);
void handleDelete(Widget *w, message *msg);
void handleFileClick(Widget *w, message *msg);
void handleKeyboard(Widget *w, message *msg);

// UI functions
void loadFiles(void);
void removeAllScrollableWidgets(void);
void initUI(void);

#endif