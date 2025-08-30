#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

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

int window_rows, window_cols;

struct termios default_term;

void ShowError(const char *message) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    printf("%s Error in %s : %s %s \n", RED, message, strerror(errno), COLOR_RESET);
    exit(1);
}

// Implement a dyncamic string data type with the capacity trick for effiency
struct string {
    char* str;
    size_t size;
    size_t capacity;
};

void StringInit(struct string* str) {
    str->size = 0;
    str->capacity = 10;
    str->str = malloc(str->capacity);    
    // Exit the program with error message if memory wasn't allocated
    if (str->str == NULL) {
        ShowError("Memory couldn't be allocated");
    }
}

void StringAppend(struct string *old, const char* add) {
    size_t add_len = strlen(add);
    if (old->capacity < (old->size + add_len)) {
        
        old->capacity = (old->size + add_len) * 2;
        old->str = realloc(old->str ,old->capacity);
    
        // Exit the program with error message if memory wasn't allocated
        if (old->str == NULL) {
            ShowError("Memory couldn't be allocated");
        }
    }

    memcpy(&old->str[old->size], add, add_len);

    old->size += add_len;
}

void StringInsert(struct string* old, size_t pos, const char* add) {
    if (pos > old->size) {
        return;
    } else if (pos == old->size) {
        StringAppend(old, add);
        return;
    }

    size_t add_len = strlen(add);
    if (old->capacity < (old->size + add_len)) {
        
        old->capacity = (old->size + add_len) * 2;
        old->str = realloc(old->str ,old->capacity);
    
        // Exit the program with error message if memory wasn't allocated
        if (old->str == NULL) {
            ShowError("Memory couldn't be allocated");
        }
    }

    // move the current string after pos to the right
    for (size_t i = old->size - 1; i >= pos; i--) {
        old->str[i+add_len] = old->str[i];
    }

    memcpy(&old->str[pos], add, add_len);
    old->size += add_len;
}

void StringDestroy(struct string* str) {
    free(str->str);
    str->str = NULL;
}


void GetWindowSize(int* window_rows, int* window_cols) {
    struct winsize window;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == -1 || window.ws_row == 0) {
        ShowError("Failed to get window size");  
    } else {
        *window_rows = window.ws_row;
        *window_cols = window.ws_col;
    }
}

void DrawTildes() {
    // color the tildes blue
    const char* tilde_cursor_pos = "\x1b[2;1H";
    struct string tildes;
    StringInit(&tildes);
    StringAppend(&tildes, tilde_cursor_pos);
    StringAppend(&tildes, BLUE);

    for (int i = 0; i < window_rows - 1; i++) {
        StringAppend(&tildes ,"\x1b[K");
        if (i == window_rows - 2) {
            StringAppend(&tildes, "~");
        } else {
            StringAppend(&tildes, "~\r\n");
        }
    }
    
    // reset the original color
    StringAppend(&tildes, COLOR_RESET);

    write(STDOUT_FILENO, tildes.str, tildes.size);
    StringDestroy(&tildes);
    
}

void EditorClearScreen() {
    write(STDOUT_FILENO, "\x1b[H", 3);

    DrawTildes();
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void DisableRawMode () {
    tcsetattr(STDIN_FILENO, TCIFLUSH, &default_term);
}

void EnableRawMode () {
    atexit(DisableRawMode);

    if (tcgetattr(STDIN_FILENO, &default_term) == -1) {
        ShowError("tcgetattr");
    }
    
    struct termios raw = default_term;

    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);


    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;


    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}


int EditorReadKey() {
    int read_ret;
    char c;
    do {
        read_ret = read(STDIN_FILENO, &c, 1);
    } while (read_ret == 0);

    if (read_ret == -1) {
        ShowError("read");
    }
    return c;
}

void EditorProccessKey() {
    int key = EditorReadKey();

    // Exit the program when entering CTRL + q
    if (key == CTRL_KEY('q')) {

        // clear the screen before exiting
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

        exit(0);
    }
}


void ShowText() {
    struct string test;
    StringInit(&test);
    StringAppend(&test, "test");
    StringInsert(&test, 1, "1");
    printf("%s \r\n %s", test.str, "\x1b[1;2H");
    fflush(stdout);

    StringDestroy(&test);
}

int main() {
    GetWindowSize(&window_rows, &window_cols);
    EnableRawMode();
    while (1) {
        EditorClearScreen();
        ShowText();
        EditorProccessKey();
    }
}
