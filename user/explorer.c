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

window desktop;
char current_path[MAX_LONG_STRLEN];
int current_path_widget;
int toolbar_bg_widget;
int input_widget = -1;
int rename_widget = -1;
int create_btn_widget = -1;
int rename_btn_widget = -1;
int selected_widget = -1;
char selected_name[MAX_SHORT_STRLEN];
int is_selected_dir = 0;
int selected_x = 0;
int selected_y = 0;
int selected_w = 0;
int selected_h = 0;
int selected_index = -1;  // Track which item is selected (0-based)
int total_items = 0;      // Total number of files/folders displayed

struct RGBA textColor;
struct RGBA dirColor;
struct RGBA bgColor;
struct RGBA buttonColor;
struct RGBA whiteColor;
struct RGBA selectedColor;

int toolbarHeight = 95;
int itemStartY = 100;

char *GUI_programs[] = {"terminal", "editor", "explorer", "floppybird"};

void gui_ls(char *path);
void refreshView();
void clearInputWidgets();
void clearRenameWidgets();
void clearSelection();
int canOpenWithEditor(char *filename);
void selectItemByIndex(int index);
void explorerKeyHandler(Widget *widget, message *msg);

// Get file extension
char *getFileExtension(char *filename) {
	static char buf[DIRSIZ + 1];
	char *p;
	for (p = filename + strlen(filename); p >= filename && *p != '.'; p--)
		;
	p++;
	if (strlen(p) >= DIRSIZ)
		return p;
	memmove(buf, p, strlen(p));
	memset(buf + strlen(p), '\0', 1);
	return buf;
}

// Check if file is openable
int isOpenable(char *filename) {
	char *allowed[] = {"terminal", "editor", "explorer", "floppybird"};
	for (int i = 0; i < 4; i++) {
		if (strcmp(filename, allowed[i]) == 0)
			return 1;
	}
	return 0;
}

// Check if file should be shown in explorer
int shouldShowFile(char *filename) {
	// List of GUI applications that should be shown
	char *guiApps[] = {"terminal", "editor", "explorer", "floppybird"};
	for (int i = 0; i < 4; i++) {
		if (strcmp(filename, guiApps[i]) == 0)
			return 1;
	}
	
	// Show text files with .txt extension
	if (strcmp(getFileExtension(filename), "txt") == 0)
		return 1;
	
	// Show markdown files and files without extension (like README.md, LICENSE)
	char *ext = getFileExtension(filename);
	if (strcmp(ext, "md") == 0)
		return 1;
	
	// Show files with uppercase letters (likely user files like README, LICENSE)
	int hasUpperCase = 0;
	for (int i = 0; filename[i] != '\0'; i++) {
		if (filename[i] >= 'A' && filename[i] <= 'Z') {
			hasUpperCase = 1;
			break;
		}
	}
	if (hasUpperCase)
		return 1;
	
	return 0;
}

// Check if file can be opened with editor
int canOpenWithEditor(char *filename) {
	char *ext = getFileExtension(filename);
	
	// .txt files
	if (strcmp(ext, "txt") == 0)
		return 1;
	
	// .md files
	if (strcmp(ext, "md") == 0)
		return 1;
	
	// Files with uppercase (README, LICENSE, etc)
	for (int i = 0; filename[i] != '\0'; i++) {
		if (filename[i] >= 'A' && filename[i] <= 'Z') {
			return 1;
		}
	}
	
	return 0;
}

// Check if file/folder is protected (system files)
int isProtected(char *name) {
	// Remove [D] prefix if exists
	char *actualName = name;
	if (name[0] == '[' && name[1] == 'D' && name[2] == ']' && name[3] == ' ') {
		actualName = name + 4;
	}
	
	// List of protected files/folders
	char *protected[] = {"editor", "explorer", "terminal", "floppybird", ".", "..", 
			     "kernel", "initcode", "init", "cat", "echo", 
			     "forktest", "grep", "kill", "ln", "ls", "mkdir",
			     "rm", "sh", "stressfs", "usertests", "wc", "zombie"};
	
	for (int i = 0; i < 23; i++) {
		if (strcmp(actualName, protected[i]) == 0) {
			return 1;
		}
	}
	return 0;
}

// Get parent path
char *getparentpath(char *path) {
	static char buf[MAX_LONG_STRLEN];
	char *p;
	for (p = path + strlen(path); p >= path && *p != '/'; p--)
		;
	memmove(buf, path, p - path);
	buf[p - path] = '\0';
	if (strlen(buf) == 0)
		strcpy(buf, "/");
	return buf;
}

// Format filename
char *fmtname(char *path) {
	static char buf[DIRSIZ + 1];
	char *p;
	for (p = path + strlen(path); p >= path && *p != '/'; p--)
		;
	p++;
	if (strlen(p) >= DIRSIZ)
		return p;
	memmove(buf, p, strlen(p));
	memset(buf + strlen(p), '\0', 1);
	return buf;
}

// Clear selection
void clearSelection() {
	selected_widget = -1;
	is_selected_dir = 0;
	selected_x = 0;
	selected_y = 0;
	selected_w = 0;
	selected_h = 0;
	selected_index = -1;
	memset(selected_name, 0, MAX_SHORT_STRLEN);
	desktop.needsRepaint = 1;
}

// Clear input widgets
void clearInputWidgets() {
	if (input_widget != -1) {
		removeWidget(&desktop, input_widget);
		input_widget = -1;
	}
	if (create_btn_widget != -1) {
		removeWidget(&desktop, create_btn_widget);
		create_btn_widget = -1;
	}
}

// Clear rename widgets
void clearRenameWidgets() {
	if (rename_widget != -1) {
		removeWidget(&desktop, rename_widget);
		rename_widget = -1;
	}
	if (rename_btn_widget != -1) {
		removeWidget(&desktop, rename_btn_widget);
		rename_btn_widget = -1;
	}
}

// Refresh view
void refreshView() {
	clearInputWidgets();
	clearRenameWidgets();
	clearSelection();
	gui_ls(current_path);
	desktop.needsRepaint = 1;
}

// Select item by index (for keyboard navigation)
void selectItemByIndex(int index) {
	if (index < 0 || index >= total_items) return;
	
	clearRenameWidgets();
	clearInputWidgets();
	
	// Find the widget at this index
	int currentIndex = 0;
	for (int p = desktop.widgetlisthead; p != -1; p = desktop.widgets[p].next) {
		if (desktop.widgets[p].scrollable == 1 && 
		    desktop.widgets[p].type == TEXT &&
		    p != current_path_widget) {
			
			if (currentIndex == index) {
				selected_widget = p;
				selected_index = index;
				
				// Determine if it's a directory or file
				if (desktop.widgets[p].context.text->text[0] == '[' &&
				    desktop.widgets[p].context.text->text[1] == 'D') {
					is_selected_dir = 1;
					strcpy(selected_name, desktop.widgets[p].context.text->text + 4);
				} else {
					is_selected_dir = 0;
					strcpy(selected_name, desktop.widgets[p].context.text->text);
				}
				
				// Set selection box dimensions
				selected_x = desktop.widgets[p].position.xmin - 2;
				selected_y = desktop.widgets[p].position.ymin - 2;
				selected_w = desktop.widgets[p].position.xmax - desktop.widgets[p].position.xmin + 4;
				selected_h = desktop.widgets[p].position.ymax - desktop.widgets[p].position.ymin + 4;
				
				// Auto-scroll if needed
				int item_top = desktop.widgets[p].position.ymin;
				int item_bottom = desktop.widgets[p].position.ymax;
				int visible_top = itemStartY;
				int visible_bottom = desktop.height;
				
				// Scroll down if item is below visible area
				if (item_bottom + desktop.scrollOffsetY > visible_bottom) {
					desktop.scrollOffsetY = item_bottom - visible_bottom + 20;
				}
				// Scroll up if item is above visible area
				if (item_top - desktop.scrollOffsetY < visible_top) {
					desktop.scrollOffsetY = item_top - visible_top - 20;
					if (desktop.scrollOffsetY < 0) desktop.scrollOffsetY = 0;
				}
				
				desktop.needsRepaint = 1;
				break;
			}
			currentIndex++;
		}
	}
}

// Keyboard handler for arrow key navigation
void explorerKeyHandler(Widget *widget, message *msg) {
	if (msg->msg_type != M_KEY_DOWN) return;
	
	int key = msg->params[0];
	
	// Arrow Down - select next item
	if (key == KEY_DN) {
		if (selected_index < total_items - 1) {
			selectItemByIndex(selected_index + 1);
		} else if (selected_index == -1 && total_items > 0) {
			// If nothing selected, select first item
			selectItemByIndex(0);
		}
	}
	// Arrow Up - select previous item
	else if (key == KEY_UP) {
		if (selected_index > 0) {
			selectItemByIndex(selected_index - 1);
		} else if (selected_index == -1 && total_items > 0) {
			// If nothing selected, select first item
			selectItemByIndex(0);
		}
	}
	// Enter - open selected item
	else if (key == '\n') {
		if (selected_widget != -1) {
			if (is_selected_dir) {
				// Enter directory
				int current_path_length = strlen(current_path);
				if (current_path_length > 0) {
					current_path[current_path_length] = '/';
					strcpy(current_path + current_path_length + 1, selected_name);
				} else {
					strcpy(current_path, selected_name);
				}
				refreshView();
			} else {
				// Open file
				char fullPath[MAX_LONG_STRLEN];
				if (strlen(current_path) > 0) {
					strcpy(fullPath, current_path);
					strcat(fullPath, "/");
					strcat(fullPath, selected_name);
				} else {
					strcpy(fullPath, selected_name);
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
}

// Create folder confirm handler
void createFolderConfirm(Widget *widget, message *msg) {
	if ((msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) && input_widget != -1) {
		char newDir[MAX_LONG_STRLEN];
		
		// Get updated text from widget
		strcpy(newDir, desktop.widgets[input_widget].context.inputfield->text);
		
		// Build full path
		char fullPath[MAX_LONG_STRLEN];
		if (strlen(current_path) > 0) {
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, newDir);
		} else {
			strcpy(fullPath, newDir);
		}
		
		// Create directory
		mkdir(fullPath);
		
		// Refresh view properly
		refreshView();
	}
}

// Create new folder handler
void newFolderHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearInputWidgets();
		clearRenameWidgets();
		
		// Add input field
		input_widget = addInputFieldWidget(&desktop, textColor, "newfolder",
						   10, 55, 200, 18, 0, inputFieldKeyHandler);
		
		// Add button
		create_btn_widget = addButtonWidget(&desktop, textColor, buttonColor, "Create", 
						    220, 53, 60, 20, 0, createFolderConfirm);
		
		desktop.needsRepaint = 1;
	}
}

// Create file confirm handler  
void createFileConfirm(Widget *widget, message *msg) {
	if ((msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) && input_widget != -1) {
		char newFile[MAX_LONG_STRLEN];
		
		// Get updated text from widget
		strcpy(newFile, desktop.widgets[input_widget].context.inputfield->text);
		
		// Build full path
		char fullPath[MAX_LONG_STRLEN];
		if (strlen(current_path) > 0) {
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, newFile);
		} else {
			strcpy(fullPath, newFile);
		}
		
		// Create file
		int fd = open(fullPath, O_CREATE | O_WRONLY);
		if (fd >= 0) {
			close(fd);
		}
		
		// Refresh view properly
		refreshView();
	}
}

// Create new file handler
void newFileHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) {
		clearInputWidgets();
		clearRenameWidgets();
		
		// Add input field
		input_widget = addInputFieldWidget(&desktop, textColor, "newfile.txt",
						   10, 55, 200, 18, 0, inputFieldKeyHandler);
		
		// Add button
		create_btn_widget = addButtonWidget(&desktop, textColor, buttonColor, "Create", 
						    220, 53, 60, 20, 0, createFileConfirm);
		
		desktop.needsRepaint = 1;
	}
}

// Back button handler
void backHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) {
		strcpy(current_path, getparentpath(current_path));
		if (strcmp(current_path, "/") == 0)
			strcpy(current_path, "");
		refreshView();
	}
}

// Directory click handler
void cdHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK) {
		// Double click = masuk folder
		char *folderName = widget->context.text->text + 4;
		
		int current_path_length = strlen(current_path);
		if (current_path_length > 0) {
			current_path[current_path_length] = '/';
			strcpy(current_path + current_path_length + 1, folderName);
		} else {
			strcpy(current_path, folderName);
		}
		
		refreshView();
	} else if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		// Single click = select (semua item bisa di-select untuk navigasi)
		clearSelection();
		clearRenameWidgets();
		
		selected_widget = findWidgetId(&desktop, widget);
		strcpy(selected_name, widget->context.text->text + 4);
		is_selected_dir = 1;
		
		// Find index of this widget
		int currentIndex = 0;
		for (int p = desktop.widgetlisthead; p != -1; p = desktop.widgets[p].next) {
			if (desktop.widgets[p].scrollable == 1 && 
			    desktop.widgets[p].type == TEXT &&
			    p != current_path_widget) {
				if (p == selected_widget) {
					selected_index = currentIndex;
					break;
				}
				currentIndex++;
			}
		}
		
		selected_x = widget->position.xmin - 2;
		selected_y = widget->position.ymin - 2;
		selected_w = widget->position.xmax - widget->position.xmin + 4;
		selected_h = widget->position.ymax - widget->position.ymin + 4;
		
		desktop.needsRepaint = 1;
	}
}

// File click handler
void fileHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK) {
		// Double click = buka file
		char *fileName = widget->context.text->text;
		
		char fullPath[MAX_LONG_STRLEN];
		if (strlen(current_path) > 0) {
			strcpy(fullPath, current_path);
			strcat(fullPath, "/");
			strcat(fullPath, fileName);
		} else {
			strcpy(fullPath, fileName);
		}
		
		if (fork() == 0) {
			// Check if file should be opened with editor
			if (canOpenWithEditor(fileName)) {
				char *argv2[] = {"editor", fullPath, 0};
				exec(argv2[0], argv2);
			} else {
				// Execute as program
				char *argv2[] = {fileName, 0};
				exec(argv2[0], argv2);
			}
			exit();
		}
	} else if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		// Single click = select (semua item bisa di-select untuk navigasi)
		clearSelection();
		clearRenameWidgets();
		
		selected_widget = findWidgetId(&desktop, widget);
		strcpy(selected_name, widget->context.text->text);
		is_selected_dir = 0;
		
		// Find index of this widget
		int currentIndex = 0;
		for (int p = desktop.widgetlisthead; p != -1; p = desktop.widgets[p].next) {
			if (desktop.widgets[p].scrollable == 1 && 
			    desktop.widgets[p].type == TEXT &&
			    p != current_path_widget) {
				if (p == selected_widget) {
					selected_index = currentIndex;
					break;
				}
				currentIndex++;
			}
		}
		
		selected_x = widget->position.xmin - 2;
		selected_y = widget->position.ymin - 2;
		selected_w = widget->position.xmax - widget->position.xmin + 4;
		selected_h = widget->position.ymax - widget->position.ymin + 4;
		
		desktop.needsRepaint = 1;
	}
}

// Rename confirm handler
void renameConfirm(Widget *widget, message *msg) {
	if ((msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) && 
	    rename_widget != -1 && selected_widget != -1) {
		char oldPath[MAX_LONG_STRLEN];
		char newPath[MAX_LONG_STRLEN];
		
		// Get new name from input field widget
		char newName[MAX_SHORT_STRLEN];
		strcpy(newName, desktop.widgets[rename_widget].context.inputfield->text);
		
		// Validate new name
		if (strlen(newName) == 0 || strcmp(newName, selected_name) == 0) {
			clearRenameWidgets();
			return;
		}
		
		// Build old path
		if (strlen(current_path) > 0) {
			strcpy(oldPath, current_path);
			strcat(oldPath, "/");
			strcat(oldPath, selected_name);
		} else {
			strcpy(oldPath, selected_name);
		}
		
		// Build new path
		if (strlen(current_path) > 0) {
			strcpy(newPath, current_path);
			strcat(newPath, "/");
			strcat(newPath, newName);
		} else {
			strcpy(newPath, newName);
		}
		
		// Perform rename using link + unlink
		if (link(oldPath, newPath) == 0) {
			unlink(oldPath);
		}
		
		// Refresh view
		refreshView();
	}
}

// Rename handler
void renameHandler(Widget *widget, message *msg) {
	if (msg->msg_type == M_MOUSE_DBCLICK || msg->msg_type == M_MOUSE_LEFT_CLICK) {
		if (selected_widget == -1) {
			// No file/folder selected
			return;
		}
		
		// Check if selected item is protected
		char checkName[MAX_SHORT_STRLEN];
		if (is_selected_dir) {
			strcpy(checkName, "[D] ");
			strcat(checkName, selected_name);
		} else {
			strcpy(checkName, selected_name);
		}
		
		if (isProtected(checkName)) {
			// Cannot rename protected files
			return;
		}
		
		clearRenameWidgets();
		clearInputWidgets();
		
		// Show rename input with current name - position in second row
		rename_widget = addInputFieldWidget(&desktop, textColor, selected_name,
						    10, 70, 200, 18, 0, inputFieldKeyHandler);
		
		rename_btn_widget = addButtonWidget(&desktop, textColor, buttonColor, "Rename", 
						    220, 68, 60, 20, 0, renameConfirm);
		desktop.needsRepaint = 1;
	}
}

// List directory contents
void gui_ls(char *path) {
	// Update path display
	strcpy(desktop.widgets[current_path_widget].context.text->text, 
	       strlen(path) > 0 ? path : "/");
	
	// Clear file/folder widgets
	int widgetsToRemove[MAX_WIDGET_SIZE];
	int removeCount = 0;
	
	for (int p = desktop.widgetlisthead; p != -1; p = desktop.widgets[p].next) {
		if ((desktop.widgets[p].type == TEXT || desktop.widgets[p].type == SHAPE) && 
		    p != current_path_widget &&
		    p != input_widget &&
		    p != rename_widget &&
		    desktop.widgets[p].scrollable == 1) {
			widgetsToRemove[removeCount++] = p;
		}
	}
	
	for (int i = 0; i < removeCount; i++) {
		removeWidget(&desktop, widgetsToRemove[i]);
	}
	
	// Use static buffer to prevent stack overflow
	static char pathBuf[MAX_LONG_STRLEN];
	char *p;
	int fd;
	struct dirent de;
	struct stat st;
	int lineCount = 0;
	
	// Open directory
	char openPath[MAX_LONG_STRLEN];
	if (strlen(path) > 0) {
		strcpy(openPath, path);
	} else {
		strcpy(openPath, ".");
	}
	
	if ((fd = open(openPath, 0)) < 0) {
		return;
	}
	
	if (fstat(fd, &st) < 0) {
		close(fd);
		return;
	}
	
	if (st.type == T_DIR) {
		if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(pathBuf)) {
			close(fd);
			return;
		}
		
		strcpy(pathBuf, path);
		p = pathBuf + strlen(pathBuf);
		if (strlen(path) > 0)
			*p++ = '/';
		
		while (read(fd, &de, sizeof(de)) == sizeof(de)) {
			if (de.inum == 0)
				continue;
			
			memmove(p, de.name, DIRSIZ);
			p[DIRSIZ] = 0;
			
			if (stat(pathBuf, &st) < 0)
				continue;
			
			char formatName[MAX_SHORT_STRLEN];
			strcpy(formatName, fmtname(pathBuf));
			
			if (st.type == T_FILE) {
				// Only show GUI apps and user files
				if (shouldShowFile(formatName)) {
					addTextWidget(&desktop, textColor, formatName, 10,
						      itemStartY + lineCount * 20, 300, 18, 1,
						      fileHandler);
					lineCount++;
				}
			} else if (st.type == T_DIR && strcmp(formatName, ".") != 0 &&
				   strcmp(formatName, "..") != 0) {
				char folderName[MAX_SHORT_STRLEN + 4];
				strcpy(folderName, "[D] ");
				strcat(folderName, formatName);
				addTextWidget(&desktop, dirColor, folderName, 10,
					      itemStartY + lineCount * 20, 300, 18, 1,
					      cdHandler);
				lineCount++;
			}
		}
	}
	
	close(fd);
	
	// Store total items count
	total_items = lineCount;
}

int main(int argc, char *argv[]) {
	desktop.width = 520;
	desktop.height = 380;
	desktop.hasTitleBar = 1;
	createWindow(&desktop, "File Explorer");
	
	// Colors
	bgColor.R = 245;
	bgColor.G = 245;
	bgColor.B = 245;
	bgColor.A = 255;
	
	textColor.R = 20;
	textColor.G = 20;
	textColor.B = 20;
	textColor.A = 255;
	
	dirColor.R = 41;
	dirColor.G = 128;
	dirColor.B = 185;
	dirColor.A = 255;
	
	buttonColor.R = 52;
	buttonColor.G = 152;
	buttonColor.B = 219;
	buttonColor.A = 255;
	
	whiteColor.R = 255;
	whiteColor.G = 255;
	whiteColor.B = 255;
	whiteColor.A = 255;
	
	selectedColor.R = 255;
	selectedColor.G = 140;
	selectedColor.B = 0;
	selectedColor.A = 255;
	
	// Background
	addColorFillWidget(&desktop, bgColor, 0, 0, desktop.width,
			   desktop.height, 0, emptyHandler);
	
	// Toolbar background
	toolbar_bg_widget = addColorFillWidget(&desktop, whiteColor, 0, 0, 
					       desktop.width, toolbarHeight, 0, emptyHandler);
	
	// Toolbar buttons
	addButtonWidget(&desktop, textColor, buttonColor, "Back", 5, 5, 50, 25, 0,
			backHandler);
	addButtonWidget(&desktop, textColor, buttonColor, "NewDir", 60, 5, 55, 25, 0,
			newFolderHandler);
	addButtonWidget(&desktop, textColor, buttonColor, "NewFile", 120, 5, 60, 25, 0,
			newFileHandler);
	addButtonWidget(&desktop, textColor, buttonColor, "Rename", 185, 5, 60, 25, 0,
			renameHandler);
	
	// Path display
	memset(current_path, 0, MAX_LONG_STRLEN);
	current_path_widget = addTextWidget(&desktop, textColor, "/", 5, 35,
					    desktop.width - 10, 18, 0, emptyHandler);
	
	// Initial directory listing
	gui_ls(current_path);
	
	// Add invisible widget to capture keyboard events
	addColorFillWidget(&desktop, bgColor, 0, 0, 0, 0, 0, explorerKeyHandler);
	desktop.keyfocus = desktop.widgetlisttail; // Set keyboard focus
	
	while (1) {
		updateWindow(&desktop);
		
		// Draw selection border after all widgets (overlay effect)
		if (selected_widget != -1 && selected_w > 0) {
			RGB borderColor;
			borderColor.R = selectedColor.R;
			borderColor.G = selectedColor.G;
			borderColor.B = selectedColor.B;
			
			int draw_y = selected_y - desktop.scrollOffsetY;
			drawRect(&desktop, borderColor, selected_x, draw_y, selected_w, selected_h);
			drawRect(&desktop, borderColor, selected_x + 1, draw_y + 1, selected_w - 2, selected_h - 2);
		}
	}
}