#include "character.h"
#include "fcntl.h"
#include "gui.h"
#include "memlayout.h"
#include "msg.h"
#include "types.h"
#include "user.h"
#include "user_gui.h"
#include "user_handler.h"
#include "user_window.h"

char *GUI_programs[] = {"terminal", "editor", "explorer", "floppybird",
			"calculator"};

// Pipe for shell communication
int sh2gui_fd[2];
#define READBUFFERSIZE 2000
char read_buf[READBUFFERSIZE];

window programWindow;
int inputWidgetId;  // Input field (fixed bottom)
int promptWidgetId; // Prompt (fixed bottom)
int totallines = 0;
int inputOffset = 25;
int promptWidth = 70;	   // Lebar area prompt "host$> "
int bottomAreaHeight = 30; // Area untuk prompt + input di bottom

// Modern color scheme
struct RGBA bgColor;
struct RGBA promptColor;
struct RGBA textColor;
struct RGBA outputColor;

// Command structures
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5
#define MAXARGS 10

struct cmd {
	int type;
};

struct execcmd {
	int type;
	char *argv[MAXARGS];
	char *eargv[MAXARGS];
};

struct redircmd {
	int type;
	struct cmd *cmd;
	char *file;
	char *efile;
	int mode;
	int fd;
};

struct pipecmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

struct listcmd {
	int type;
	struct cmd *left;
	struct cmd *right;
};

struct backcmd {
	int type;
	struct cmd *cmd;
};

// Function declarations
int fork1(void);
void panic(char *);
struct cmd *parsecmd(char *);
void freecmd(struct cmd *);
void safestrcpy(char *dst, const char *src, int n);
void removeAllHistory(void);
void clearTerminal(void);
void addToHistory(char *text, struct RGBA color);
void executeCommand(char *buffer);

void safestrcpy(char *dst, const char *src, int n) {
	int i;
	for (i = 0; i < n - 1 && src[i]; i++)
		dst[i] = src[i];
	dst[i] = 0;
}

void freecmd(struct cmd *cmd) {
	if (!cmd)
		return;

	switch (cmd->type) {
	case EXEC:
		free(cmd);
		break;
	case REDIR: {
		struct redircmd *rcmd = (struct redircmd *)cmd;
		freecmd(rcmd->cmd);
		free(cmd);
		break;
	}
	case PIPE: {
		struct pipecmd *pcmd = (struct pipecmd *)cmd;
		freecmd(pcmd->left);
		freecmd(pcmd->right);
		free(cmd);
		break;
	}
	case LIST: {
		struct listcmd *lcmd = (struct listcmd *)cmd;
		freecmd(lcmd->left);
		freecmd(lcmd->right);
		free(cmd);
		break;
	}
	case BACK: {
		struct backcmd *bcmd = (struct backcmd *)cmd;
		freecmd(bcmd->cmd);
		free(cmd);
		break;
	}
	default:
		free(cmd);
	}
}

// Execute command
void runcmd(struct cmd *cmd) {
	int p[2];
	struct backcmd *bcmd;
	struct execcmd *ecmd;
	struct listcmd *lcmd;
	struct pipecmd *pcmd;
	struct redircmd *rcmd;

	if (cmd == 0)
		exit();

	switch (cmd->type) {
	default:
		panic("runcmd");

	case EXEC:
		ecmd = (struct execcmd *)cmd;
		if (ecmd->argv[0] == 0)
			exit();
		exec(ecmd->argv[0], ecmd->argv);
		printf(2, "exec %s failed\n", ecmd->argv[0]);
		break;

	case REDIR:
		rcmd = (struct redircmd *)cmd;
		close(rcmd->fd);
		if (open(rcmd->file, rcmd->mode) < 0) {
			printf(2, "open %s failed\n", rcmd->file);
			exit();
		}
		runcmd(rcmd->cmd);
		break;

	case LIST:
		lcmd = (struct listcmd *)cmd;
		if (fork1() == 0)
			runcmd(lcmd->left);
		wait();
		runcmd(lcmd->right);
		break;

	case PIPE:
		pcmd = (struct pipecmd *)cmd;
		if (pipe(p) < 0)
			panic("pipe");
		if (fork1() == 0) {
			close(1);
			dup(p[1]);
			close(p[0]);
			close(p[1]);
			runcmd(pcmd->left);
		}
		if (fork1() == 0) {
			close(0);
			dup(p[0]);
			close(p[0]);
			close(p[1]);
			runcmd(pcmd->right);
		}
		close(p[0]);
		close(p[1]);
		wait();
		wait();
		break;

	case BACK:
		bcmd = (struct backcmd *)cmd;
		if (fork1() == 0)
			runcmd(bcmd->cmd);
		break;
	}
	exit();
}

// Check if command is a GUI program
int isGUIProgram(char *cmd) {
	char cmdname[MAX_SHORT_STRLEN];
	int i = 0;
	while (cmd[i] && cmd[i] != ' ' && i < MAX_SHORT_STRLEN - 1) {
		cmdname[i] = cmd[i];
		i++;
	}
	cmdname[i] = '\0';

	for (int j = 0; j < sizeof(GUI_programs) / sizeof(GUI_programs[0]);
	     j++) {
		if (strcmp(cmdname, GUI_programs[j]) == 0)
			return 1;
	}
	return 0;
}

// Remove all history widgets (keep fixed widgets: bg, prompt, input)
void removeAllHistory(void) {
	int p = programWindow.widgetlisttail;
	while (p != -1 && p > inputWidgetId) {
		int prev = programWindow.widgets[p].prev;
		removeWidget(&programWindow, p);
		p = prev;
	}
}

// Clear terminal screen
void clearTerminal(void) {
	removeAllHistory();
	totallines = 0;
	programWindow.scrollOffsetY = 0;
	programWindow.needsRepaint = 1;
}

// Add text to history area (scrollable)
void addToHistory(char *text, struct RGBA color) {
	int width = programWindow.width - inputOffset * 2;
	int textLen = strlen(text);
	int lines = getMouseYFromOffset(text, width, textLen) + 1;
	int height = lines * CHARACTER_HEIGHT;

	// Y position: start from top with margin
	int yPos = 30 + (totallines * CHARACTER_HEIGHT);

	addTextWidget(&programWindow, color, text, inputOffset, yPos, width,
		      height, 1, emptyHandler);

	totallines += lines;

	// Auto-scroll to show latest history
	int totalContentHeight = 30 + (totallines * CHARACTER_HEIGHT);
	int visibleHeight = programWindow.height - bottomAreaHeight - 30;
	if (totalContentHeight > visibleHeight) {
		programWindow.scrollOffsetY =
			totalContentHeight - visibleHeight;
	}
}

// Execute command and handle output
void executeCommand(char *buffer) {
	// Check for built-in commands first
	if (strcmp(buffer, "clear") == 0) {
		clearTerminal();
		return;
	}

	int isGUI = isGUIProgram(buffer);
	int pipe_fds[2] = {-1, -1};

	if (!isGUI) {
		if (pipe(pipe_fds) < 0) {
			pipe_fds[0] = -1;
			pipe_fds[1] = -1;
		}
	}

	int pid = fork();
	if (pid < 0) {
		if (pipe_fds[0] >= 0) {
			close(pipe_fds[0]);
			close(pipe_fds[1]);
		}
	} else if (pid == 0) {
		// Child
		if (!isGUI && pipe_fds[1] >= 0) {
			close(pipe_fds[0]);
			close(1);
			dup(pipe_fds[1]);
			close(pipe_fds[1]);
		}
		struct cmd *cmd = parsecmd(buffer);
		if (cmd) {
			runcmd(cmd);
			freecmd(cmd);
		}
		exit();
	} else {
		// Parent
		memset(read_buf, 0, READBUFFERSIZE);

		if (!isGUI && pipe_fds[0] >= 0) {
			close(pipe_fds[1]);
			int n, totalRead = 0;
			char tempBuf[256];

			while (totalRead < READBUFFERSIZE - 256) {
				n = read(pipe_fds[0], tempBuf, 255);
				if (n <= 0)
					break;
				tempBuf[n] = '\0';
				if (totalRead + n < READBUFFERSIZE - 1) {
					memmove(read_buf + totalRead, tempBuf,
						n);
					totalRead += n;
					read_buf[totalRead] = '\0';
				}
			}
			close(pipe_fds[0]);
		}
		wait();
	}

	// Add command to history (with prompt style)
	char historyLine[MAX_LONG_STRLEN];
	strcpy(historyLine, "host$> ");
	int promptLen = strlen(historyLine);
	int cmdLen = strlen(buffer);
	if (promptLen + cmdLen < MAX_LONG_STRLEN - 1) {
		strcpy(historyLine + promptLen, buffer);
	}

	addToHistory(historyLine, promptColor);

	// Add output if exists
	if (strlen(read_buf) > 0) {
		addToHistory(read_buf, outputColor);
	}
}

// Input handler
void inputHandler(Widget *w, message *msg) {
	if (!w || !w->context.inputfield)
		return;

	int width = w->position.xmax - w->position.xmin;
	int charCount = strlen(w->context.inputfield->text);

	if (msg->msg_type == M_MOUSE_LEFT_CLICK) {
		inputMouseLeftClickHandler(w, msg);
	} else if (msg->msg_type == M_KEY_DOWN) {
		int c = msg->params[0];
		char buffer[MAX_LONG_STRLEN];

		if (c == '\n') {
			// Copy command
			safestrcpy(buffer, w->context.inputfield->text,
				   MAX_LONG_STRLEN);

			// Skip if empty
			if (strlen(buffer) == 0) {
				return;
			}

			// Execute command
			executeCommand(buffer);

			// Clear input field
			w->context.inputfield->text[0] = '\0';
			w->context.inputfield->current_pos = 0;

			programWindow.needsRepaint = 1;

		} else {
			// Handle typing
			if (charCount < MAX_LONG_STRLEN - 1) {
				inputFieldKeyHandler(w, msg);
			}

			// Auto-grow height if multiline
			int textLines =
				getMouseYFromOffset(
					w->context.inputfield->text, width,
					strlen(w->context.inputfield->text)) +
				1;
			int newHeight = textLines * CHARACTER_HEIGHT;
			int maxInputHeight = bottomAreaHeight - 5;
			if (newHeight > CHARACTER_HEIGHT &&
			    newHeight < maxInputHeight) {
				w->position.ymax = w->position.ymin + newHeight;
			}
		}
	}
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	// Window configuration
	programWindow.width = 600;
	programWindow.height = 450;
	programWindow.hasTitleBar = 1;
	createWindow(&programWindow, "Terminal");

	int width = programWindow.width - inputOffset * 2;

	// Colors
	bgColor.R = 30;
	bgColor.G = 30;
	bgColor.B = 30;
	bgColor.A = 255;
	promptColor.R = 76;
	promptColor.G = 175;
	promptColor.B = 80;
	promptColor.A = 255;
	textColor.R = 240;
	textColor.G = 240;
	textColor.B = 240;
	textColor.A = 255;
	outputColor.R = 200;
	outputColor.G = 200;
	outputColor.B = 200;
	outputColor.A = 255;

	// Background for history area (top, scrollable)
	addColorFillWidget(&programWindow, bgColor, 0, 0, programWindow.width,
			   programWindow.height - bottomAreaHeight, 0,
			   emptyHandler);

	// Background for input area (bottom, fixed)
	struct RGBA inputBgColor = bgColor;
	inputBgColor.R = 40;
	inputBgColor.G = 40;
	inputBgColor.B = 40;
	addColorFillWidget(&programWindow, inputBgColor, 0,
			   programWindow.height - bottomAreaHeight,
			   programWindow.width, bottomAreaHeight, 0,
			   emptyHandler);

	// Create prompt widget (fixed bottom)
	int bottomY = programWindow.height - bottomAreaHeight;
	promptWidgetId = addTextWidget(&programWindow, promptColor, "host$>",
				       inputOffset, bottomY + 5, promptWidth,
				       CHARACTER_HEIGHT, 0, emptyHandler);

	// Create input field (fixed bottom, next to prompt)
	inputWidgetId = addInputFieldWidget(&programWindow, textColor, "",
					    inputOffset + promptWidth,
					    bottomY + 5, width - promptWidth,
					    CHARACTER_HEIGHT, 0, inputHandler);

	printf(1, "Terminal started\n");

	while (1) {
		updateWindow(&programWindow);
	}

	return 0;
}

void panic(char *s) {
	printf(2, "%s\n", s);
	exit();
}

int fork1(void) {
	int pid;
	pid = fork();
	if (pid == -1)
		panic("fork");
	return pid;
}

// Command constructors
struct cmd *execcmd(void) {
	struct execcmd *cmd;
	cmd = malloc(sizeof(*cmd));
	if (!cmd)
		panic("malloc failed");
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = EXEC;
	return (struct cmd *)cmd;
}

struct cmd *redircmd(struct cmd *subcmd, char *file, char *efile, int mode,
		     int fd) {
	struct redircmd *cmd;
	cmd = malloc(sizeof(*cmd));
	if (!cmd)
		panic("malloc failed");
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = REDIR;
	cmd->cmd = subcmd;
	cmd->file = file;
	cmd->efile = efile;
	cmd->mode = mode;
	cmd->fd = fd;
	return (struct cmd *)cmd;
}

struct cmd *pipecmd(struct cmd *left, struct cmd *right) {
	struct pipecmd *cmd;
	cmd = malloc(sizeof(*cmd));
	if (!cmd)
		panic("malloc failed");
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = PIPE;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd *)cmd;
}

struct cmd *listcmd(struct cmd *left, struct cmd *right) {
	struct listcmd *cmd;
	cmd = malloc(sizeof(*cmd));
	if (!cmd)
		panic("malloc failed");
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = LIST;
	cmd->left = left;
	cmd->right = right;
	return (struct cmd *)cmd;
}

struct cmd *backcmd(struct cmd *subcmd) {
	struct backcmd *cmd;
	cmd = malloc(sizeof(*cmd));
	if (!cmd)
		panic("malloc failed");
	memset(cmd, 0, sizeof(*cmd));
	cmd->type = BACK;
	cmd->cmd = subcmd;
	return (struct cmd *)cmd;
}

// Parsing
char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq) {
	char *s;
	int ret;

	s = *ps;
	while (s < es && strchr(whitespace, *s))
		s++;
	if (q)
		*q = s;
	ret = *s;
	switch (*s) {
	case 0:
		break;
	case '|':
	case '(':
	case ')':
	case ';':
	case '&':
	case '<':
		s++;
		break;
	case '>':
		s++;
		if (*s == '>') {
			ret = '+';
			s++;
		}
		break;
	default:
		ret = 'a';
		while (s < es && !strchr(whitespace, *s) &&
		       !strchr(symbols, *s))
			s++;
		break;
	}
	if (eq)
		*eq = s;

	while (s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return ret;
}

int peek(char **ps, char *es, char *toks) {
	char *s;
	s = *ps;
	while (s < es && strchr(whitespace, *s))
		s++;
	*ps = s;
	return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *parsecmd(char *s) {
	char *es;
	struct cmd *cmd;

	if (!s || strlen(s) == 0)
		return 0;

	es = s + strlen(s);
	cmd = parseline(&s, es);
	peek(&s, es, "");
	if (s != es) {
		printf(2, "leftovers: %s\n", s);
		panic("syntax");
	}
	nulterminate(cmd);
	return cmd;
}

struct cmd *parseline(char **ps, char *es) {
	struct cmd *cmd;
	cmd = parsepipe(ps, es);
	while (peek(ps, es, "&")) {
		gettoken(ps, es, 0, 0);
		cmd = backcmd(cmd);
	}
	if (peek(ps, es, ";")) {
		gettoken(ps, es, 0, 0);
		cmd = listcmd(cmd, parseline(ps, es));
	}
	return cmd;
}

struct cmd *parsepipe(char **ps, char *es) {
	struct cmd *cmd;
	cmd = parseexec(ps, es);
	if (peek(ps, es, "|")) {
		gettoken(ps, es, 0, 0);
		cmd = pipecmd(cmd, parsepipe(ps, es));
	}
	return cmd;
}

struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es) {
	int tok;
	char *q, *eq;

	while (peek(ps, es, "<>")) {
		tok = gettoken(ps, es, 0, 0);
		if (gettoken(ps, es, &q, &eq) != 'a')
			panic("missing file for redirection");
		switch (tok) {
		case '<':
			cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
			break;
		case '>':
			cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
			break;
		case '+':
			cmd = redircmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
			break;
		}
	}
	return cmd;
}

struct cmd *parseblock(char **ps, char *es) {
	struct cmd *cmd;
	if (!peek(ps, es, "("))
		panic("parseblock");
	gettoken(ps, es, 0, 0);
	cmd = parseline(ps, es);
	if (!peek(ps, es, ")"))
		panic("syntax - missing )");
	gettoken(ps, es, 0, 0);
	cmd = parseredirs(cmd, ps, es);
	return cmd;
}

struct cmd *parseexec(char **ps, char *es) {
	char *q, *eq;
	int tok, argc;
	struct execcmd *cmd;
	struct cmd *ret;

	if (peek(ps, es, "("))
		return parseblock(ps, es);

	ret = execcmd();
	cmd = (struct execcmd *)ret;

	argc = 0;
	ret = parseredirs(ret, ps, es);
	while (!peek(ps, es, "|)&;")) {
		if ((tok = gettoken(ps, es, &q, &eq)) == 0)
			break;
		if (tok != 'a')
			panic("syntax");
		cmd->argv[argc] = q;
		cmd->eargv[argc] = eq;
		argc++;
		if (argc >= MAXARGS)
			panic("too many args");
		ret = parseredirs(ret, ps, es);
	}
	cmd->argv[argc] = 0;
	cmd->eargv[argc] = 0;
	return ret;
}

struct cmd *nulterminate(struct cmd *cmd) {
	int i;
	struct backcmd *bcmd;
	struct execcmd *ecmd;
	struct listcmd *lcmd;
	struct pipecmd *pcmd;
	struct redircmd *rcmd;

	if (cmd == 0)
		return 0;

	switch (cmd->type) {
	case EXEC:
		ecmd = (struct execcmd *)cmd;
		for (i = 0; ecmd->argv[i]; i++)
			*ecmd->eargv[i] = 0;
		break;

	case REDIR:
		rcmd = (struct redircmd *)cmd;
		nulterminate(rcmd->cmd);
		*rcmd->efile = 0;
		break;

	case PIPE:
		pcmd = (struct pipecmd *)cmd;
		nulterminate(pcmd->left);
		nulterminate(pcmd->right);
		break;

	case LIST:
		lcmd = (struct listcmd *)cmd;
		nulterminate(lcmd->left);
		nulterminate(lcmd->right);
		break;

	case BACK:
		bcmd = (struct backcmd *)cmd;
		nulterminate(bcmd->cmd);
		break;
	}
	return cmd;
}