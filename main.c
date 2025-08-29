#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// getting ctrl combinations
#define CTRL_KEY(c) (c & (31))

// define ANSI Codes for colors
#define  BLACK       "\x1b[30m" 
#define  RED         "\x1b[31m" 
#define  GREEN       "\x1b[32m" 
#define  YELLOW      "\x1b[33m" 
#define  BLUE        "\x1b[34m" 
#define  MAGNETA     "\x1b[35m" 
#define  CYAN        "\x1b[36m" 
#define  WHITE       "\x1b[37m" 
#define  COLOR_RESET "\x1b[0m"   


struct termios orig_termios;

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[H", 3);
    write(STDOUT_FILENO, "\x1b[2J", 4);

    perror(s);
    exit(1);
}

void DrawTildes() {
    // Color this tildes BLUE
    const char* tilde_cursor_pos = "\x1b[2;1H";
    write(STDOUT_FILENO ,tilde_cursor_pos, strlen(tilde_cursor_pos));
    write(STDOUT_FILENO, BLUE, strlen(BLUE));
    
    for (int i = 0; i < 24; i++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
    // reset the original color

    write(STDOUT_FILENO, COLOR_RESET, strlen(COLOR_RESET));
    
}

void EditorClearScreen() {
    write(STDOUT_FILENO, "\x1b[H", 3);
    write(STDOUT_FILENO, "\x1b[2J", 4);

    DrawTildes();
    write(STDOUT_FILENO, "\x1b[H", 3);
}

  
void DisableRawMode () {
    if (tcsetattr(STDERR_FILENO, TCIFLUSH, &orig_termios) == -1) {
        die("tcsetattr");
    }
    tcsetattr(STDERR_FILENO, TCIFLUSH, &orig_termios);
}



void EnableRawMode () {

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        die("tcgetattr");
    }
    

    struct termios raw = orig_termios;

    atexit(DisableRawMode);

    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);


    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;


    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcgetattr");
    }
}


int EditorReadKey() {
    int read_ret;
    char c;
    do {
        read_ret = read(STDIN_FILENO, &c, 1);
    } while (read_ret == 0);

    if (read_ret == -1) {
        die("read");
    }
    return c;
}

void EditorProccessKey() {
    int key = EditorReadKey();
    if (key == CTRL_KEY('q')) {
        write(STDOUT_FILENO, "\x1b[H", 3);
        write(STDOUT_FILENO, "\x1b[2J", 4);
    
        exit(0);
    }
}

int main() {
    EnableRawMode();
    while (1) {
        EditorClearScreen();
        EditorProccessKey();
    }
}
