#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef DOS 
	#include <conio.h>
	#define IS_DOS 1
#else
	#include <unistd.h>
	#include <termios.h>
	#include <string.h>
	#define IS_DOS 0
	struct termios orig_termios;
#endif

#define BOMB 1
#define SAFE 0 

#define ROWS 16
#define COLS 30

#define NUM_BOMS 99

int START_Y = 0;
int START_X = 0;

int cursor_y;
int cursor_x;
char gameover = 0;
char draw = 1;

int grid[ROWS][COLS];
int opened[ROWS][COLS];

// ========== PLATFORM SPECIFIC HELPERS ==========

#if IS_DOS

void gotoXY(int x, int y) {
	gotoxy(x, y);
}

void printChar(const char *str, int color) {
	if(color >= 0) textcolor(color);
	cprintf("%s", str);
}

#else

void disableRawMode() {
	tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
	printf("\033[?25h");
}

void enableRawMode() {
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(disableRawMode);

	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);

	printf("\033[?25l");
}

char getKey() {
	char c;
	if(read(STDIN_FILENO, &c, 1) == 1) return c;
	return 0;
}

void clearScreen() {
	printf("\033[2J\033[H");
}

void gotoXY(int x, int y) {
	printf("\033[%d;%dH", y, x);
}

void printChar(const char *str, int color) {
	const char *colors[] = {
		"\033[0m",   // 0 reset
		"\033[34m",  // 1 blue
		"\033[32m",  // 2 green
		"\033[36m",  // 3 cyan
		"\033[31m",  // 4 red
		"\033[35m",  // 5 purple
		"\033[33m",  // 6 brown/yellow
		"\033[37m",  // 7 white
		"\033[90m"   // 8 gray
	};
	
	if(color >= 0 && color <= 8)
		printf("%s%s\033[0m", colors[color], str);
	else
		printf("%s", str);
}

void getTerminalSize(int *rows, int *cols) {
	// Save current terminal state
	struct termios saved, raw;
	tcgetattr(STDIN_FILENO, &saved);
	
	// Enable raw mode temporarily
	raw = saved;
	raw.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	
	// Query cursor position by moving to bottom-right and asking position
	printf("\033[999;999H");  // Move cursor to far bottom-right
	printf("\033[6n");        // Query cursor position
	fflush(stdout);
	
	// Read response: ESC [ rows ; cols R
	*rows = 24; *cols = 80;  // Default fallback
	char buf[32];
	int i = 0;
	
	while(i < sizeof(buf) - 1) {
		if(read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if(buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';
	
	// Parse response
	if(i > 2 && buf[0] == '\033' && buf[1] == '[') {
		sscanf(buf + 2, "%d;%d", rows, cols);
	}
	
	// Restore terminal state
	tcsetattr(STDIN_FILENO, TCSANOW, &saved);
}

#endif

// ========== COMMON FUNCTIONS ==========

int countBombs(int y, int x) {
	int bombs = 0;
	for(int dy = -1; dy <= 1; dy++) {
		for(int dx = -1; dx <= 1; dx++) {
			int ny = y + dy;
			int nx = x + dx;
			if(ny >= 0 && ny < ROWS && nx >= 0 && nx < COLS && grid[ny][nx] == BOMB)
				bombs++;
		}
	}
	return bombs;
}

void openCell(int y, int x) {
	if(y < 0 || y >= ROWS || x < 0 || x >= COLS) return;
	if(opened[y][x]) return;
	if(grid[y][x] == BOMB) return;

	opened[y][x] = 1;

	int count = countBombs(y, x);
	if(count > 0) return;

	for(int dy = -1; dy <= 1; dy++) {
		for(int dx = -1; dx <= 1; dx++) {
			if(dy != 0 || dx != 0)
				openCell(y + dy, x + dx);
		}
	}
}

void drawGrid() {
	if(!draw) return;
	draw = 0;
	
	for(int y = 0; y < ROWS; y++) {
		for(int x = 0; x < COLS; x++) {
			int posY = START_Y + y;
			int posX = START_X + x;
			
			gotoXY(posX, posY);
			
			if(posY == cursor_y && posX == cursor_x) {
				printChar("@", gameover ? 4 : 2);
			} else if(opened[y][x] == 0) {
				if(gameover && grid[y][x] == BOMB)
					printChar("*", 4);
				else
					printChar("X", 7);
			} else {
				int count = countBombs(y, x);
				if(count == 0) {
					printChar("+", 8);
				} else {
					char buf[2];
					sprintf(buf, "%d", count);
					printChar(buf, count);
				}
			}
		}
	}
	
#if !IS_DOS
	fflush(stdout);
#endif
}

char readKeyboard() {
#if IS_DOS
	if(kbhit()) {
		char key = getch();
		if(gameover) return key;
		if(key == 0 || key == -32) {
			draw = 1;
			key = getch();
			switch(key) {
			case 72: cursor_y = (cursor_y > START_Y) ? cursor_y - 1 : START_Y; break;
			case 80: cursor_y = (cursor_y < START_Y + ROWS - 1) ? cursor_y + 1 : START_Y + ROWS - 1; break;
			case 75: cursor_x = (cursor_x > START_X) ? cursor_x - 1 : START_X; break;
			case 77: cursor_x = (cursor_x < START_X + COLS - 1) ? cursor_x + 1 : START_X + COLS - 1; break;
			}
		} else {
			draw = 1;
			switch(key) {
			case 'w': cursor_y = (cursor_y > START_Y) ? cursor_y - 1 : START_Y; break;
			case 's': cursor_y = (cursor_y < START_Y + ROWS - 1) ? cursor_y + 1 : START_Y + ROWS - 1; break;
			case 'a': cursor_x = (cursor_x > START_X) ? cursor_x - 1 : START_X; break;
			case 'd': cursor_x = (cursor_x < START_X + COLS - 1) ? cursor_x + 1 : START_X + COLS - 1; break;
			}
		}
		return key;
	}
	return 0;
#else
	char key = getKey();
	if(gameover) return key;
	if(key == '\033') {
		draw = 1;
		char seq[2];
		read(STDIN_FILENO, &seq, 2);
		if(seq[0] == '[') {
			switch(seq[1]) {
			case 'A': cursor_y = (cursor_y > START_Y) ? cursor_y - 1 : START_Y; break;
			case 'B': cursor_y = (cursor_y < START_Y + ROWS - 1) ? cursor_y + 1 : START_Y + ROWS - 1; break;
			case 'C': cursor_x = (cursor_x < START_X + COLS - 1) ? cursor_x + 1 : START_X + COLS - 1; break;
			case 'D': cursor_x = (cursor_x > START_X) ? cursor_x - 1 : START_X; break;
			}
		}
	} else if(key) {
		draw = 1;
		switch(key) {
		case 'w': cursor_y = (cursor_y > START_Y) ? cursor_y - 1 : START_Y; break;
		case 'a': cursor_x = (cursor_x > START_X) ? cursor_x - 1 : START_X; break;
		case 's': cursor_y = (cursor_y < START_Y + ROWS - 1) ? cursor_y + 1 : START_Y + ROWS - 1; break;
		case 'd': cursor_x = (cursor_x < START_X + COLS - 1) ? cursor_x + 1 : START_X + COLS - 1; break;
		}
	}
	return key;
#endif
}

// ========== MAIN ==========

int main() {
#if IS_DOS
	struct text_info ti;
	gettextinfo(&ti);

	START_X = (ti.screenwidth - COLS) / 2 + 1;
	START_Y = (ti.screenheight - ROWS) / 2 + 1;
#else
	int term_rows, term_cols;
	getTerminalSize(&term_rows, &term_cols);
	
	START_X = (term_cols - COLS) / 2 + 1;
	START_Y = (term_rows - ROWS) / 2 + 1;
	
	// Ensure minimum values
	if(START_X < 1) START_X = 1;
	if(START_Y < 1) START_Y = 1;
#endif

	cursor_x = START_X;
	cursor_y = START_Y;

	for(int y = 0; y < ROWS; y++) {
		for(int x = 0; x < COLS; x++) {
			grid[y][x] = SAFE;
			opened[y][x] = 0;
		}
	}

	srand(time(NULL));
	
	int placed = 0;
	while(placed < NUM_BOMS) {
		int y = rand() % ROWS;
		int x = rand() % COLS;
		if(grid[y][x] != BOMB) {
			grid[y][x] = BOMB;
			placed++;
		}
	}

#if IS_DOS
	clrscr();
#else
	enableRawMode();
	clearScreen();
#endif

	while(1) {
		if(!gameover) {
			char key = readKeyboard();
			if(key == 'q') {
#if !IS_DOS
				clearScreen();
#else
				clrscr();
#endif
				exit(0);
			}
			if(key == ' ') {
				int y = cursor_y - START_Y;
				int x = cursor_x - START_X;
				if(grid[y][x] == BOMB) {
					gameover = 1;
					draw = 1;
					drawGrid();
#if IS_DOS
					gotoxy(1, START_Y + ROWS + 1);
					cprintf("Game Over! Press Q to quit.");
#else
					printf("\033[%d;1HGame Over! Press Q to quit.", START_Y + ROWS + 1);
					fflush(stdout);
#endif
					continue;
				} else {
					openCell(y, x);
				}
			}
			drawGrid();
		} else {
#if IS_DOS
			if(kbhit()) {
				char key = getch();
				if(key == 'q') {
					clrscr();
					exit(0);
				}
			}
#else
			char key = getKey();
			if(key == 'q') {
				clearScreen();
				exit(0);
			}
#endif
		}
		
#if !IS_DOS
		usleep(16000);
#endif
	}

	return 0;
}
