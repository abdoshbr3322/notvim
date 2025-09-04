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

// some useful macros
#define ESC '\x1b' // Escape Sequence

// math utils
int ceil_d(int a, int b) {
    if (b == 0) return 0;
    return (a / b) + (a % b != 0);
} 

// define keys enum
enum KEYS {
    CURSOR_LEFT = 'h',
    CURSOR_RIGHT = 'l',
    CURSOR_UP = 'k',
    CURSOR_DOWN = 'j'
};

// Shows error message and exits
void ShowError(const char *message) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    printf("%s Error in %s : %s %s \n", RED, message, strerror(errno), COLOR_RESET);
    exit(1);
}

// Implement a dyncamic string data type with the capacity trick for effiency
typedef struct 
{
    char* str;
    size_t size;
    size_t capacity;
} String;

String* StringInit() {
    String* str = malloc(sizeof(String));
    if (str == NULL) {
        ShowError("Memory couldn't be allocated");
    }

    // init
    str->size = 0;
    str->capacity = 10;
    str->str = malloc(str->capacity);    
    if (str->str == NULL) {
        free(str);
        ShowError("Memory couldn't be allocated");
    }

    // insert null terminator
    str->str[str->size] = 0;

    // Exit the program with error message if memory wasn't allocated
    if (str->str == NULL) {
        ShowError("Memory couldn't be allocated");
    }
    return str;
}

void StringAppend(String *old, const char* add) {
    size_t add_len = strlen(add);
    if (old->capacity <= (old->size + add_len)) {
        
        old->capacity = (old->size + add_len) * 2;
        old->str = realloc(old->str ,old->capacity);
    
        // Exit the program with error message if memory wasn't allocated
        if (old->str == NULL) {
            ShowError("Memory couldn't be allocated");
        }
    }

    memcpy(&old->str[old->size], add, add_len);
    old->size += add_len;
    old->str[old->size] = 0;
}

void StringInsert(String* old, size_t pos, const char* add) {
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
    old->str[old->size] = 0;
}

void StringDestroy(String* str) {
    free(str->str);
    free(str);
}

// Editor Configuration
typedef struct
{
    int window_rows, window_cols;
    int cursor_x, cursor_y;
    struct termios default_term;
    int file_opened;
    String* file_name, *buffer;
    int start_line;
} Editor;

Editor editor;

void EditorInit() {
    editor.cursor_x = editor.cursor_y = 1;
    if (tcgetattr(STDIN_FILENO, &editor.default_term) == -1) {
        ShowError("tcgetattr");
    }
    editor.start_line = 0;
    editor.file_opened = 0;
    editor.file_name = StringInit();
    editor.buffer = StringInit();
}

void EditorDestroy() {
    StringDestroy(editor.file_name);
    StringDestroy(editor.buffer);
}

// make a dyncamic array to store lines
typedef struct 
{
    size_t size;
    size_t capacity;
    String** array;
} ArrayBuffer;

ArrayBuffer array_buffer;

void BufferInit() {
    array_buffer.size = 0;
    array_buffer.capacity = 10;
    array_buffer.array = (String**)malloc(array_buffer.capacity * sizeof(String*));
}

void BufferExpandCapacity() {
    array_buffer.capacity *= 2;
    array_buffer.array = (String**)realloc(array_buffer.array, array_buffer.capacity * sizeof(String*));
}

void BufferAppendLine(const char* add) {
    if (array_buffer.size == array_buffer.capacity) 
        BufferExpandCapacity();
    
    String* add_line = StringInit();
    StringAppend(add_line, add);
    array_buffer.array[array_buffer.size++] = add_line; 
}

// TODO : a function to add white spaces

// void BufferInserySpace(int idx) {

// }

void BufferDestroy() {
    for (size_t i = 0; i < array_buffer.size; i++) {
        StringDestroy(array_buffer.array[i]);
    }
    free(array_buffer.array);
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

void DisableRawMode () {
    tcsetattr(STDIN_FILENO, TCIFLUSH, &editor.default_term);
}

void EnableRawMode () {
    atexit(DisableRawMode);
    
    struct termios raw = editor.default_term;

    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);


    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;


    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void DrawTildes() {
    // color the tildes blue
    const char* tilde_cursor_pos = "\x1b[H";
    String* tildes = StringInit();
    StringAppend(tildes, tilde_cursor_pos);
    StringAppend(tildes, BLUE);

    for (int i = 0; i < editor.window_rows - 1; i++) {
        StringAppend(tildes ,"\x1b[K");
        if (i == editor.window_rows - 2) {
            StringAppend(tildes, "~");
        } else {
            StringAppend(tildes, "~\r\n");
        }
    }
    
    // reset the original color
    StringAppend(tildes, COLOR_RESET);

    write(STDOUT_FILENO, tildes->str, tildes->size);
    StringDestroy(tildes);
    
}

void ShowWelcomeMessage() {
    const char* message = "~ Welcome To notvim ~";
    int y_pos = (editor.window_rows / 2);
    int x_pos = (editor.window_cols / 2) - strlen(message) / 2;

    String* message_buffer = StringInit();
    char message_pos[20];
    snprintf(message_pos, 20, "\x1b[%d;%dH", y_pos, x_pos);
    StringAppend(message_buffer, message_pos);
    StringAppend(message_buffer, message);

    write(STDOUT_FILENO, message_buffer->str, message_buffer->size);

    StringDestroy(message_buffer);
}


void ShowTextFromBuffer() {
    int lines_needed = 0;
    String* lines = StringInit();
    StringAppend(lines,  "\x1b[H");
    for (size_t line = 0; line < array_buffer.size; line++) {
        String* cur_line = array_buffer.array[line];

        // calculate number of lines in the terminal needed to render the current line
        int needed = (cur_line->size ? ceil_d(cur_line->size, editor.window_cols) : 1);

        // stop rendering when the terminal is full
        lines_needed += needed;
        if (lines_needed >= editor.window_rows) break;

        StringAppend(lines ,"\x1b[K");
        StringAppend(lines ,cur_line->str);
        StringAppend(lines, "\r\n");
    }
    write(STDOUT_FILENO ,lines->str, lines->size);
    StringDestroy(lines);
}

void ReadFileToBuffer(const char *filename) { 
    
    // change the global state for the file
    editor.file_opened = 1;
    StringAppend(editor.file_name, filename);
    
    FILE *fptr;
    char* buffer = NULL;
    size_t len;

    // Open the file in read mode
    fptr = fopen(filename, "r");

    while (getline(&buffer, &len, fptr) != -1) {
        buffer[strlen(buffer) - 1] = '\0';
        BufferAppendLine(buffer);
    }

    fclose(fptr);

}


void EditorClearScreen() {
    DrawTildes();
    if (editor.file_opened == 0) {
        ShowWelcomeMessage();
    } else {
        ShowTextFromBuffer();
    }
    char cursor_pos[20];
    snprintf(cursor_pos, sizeof(cursor_pos), "\x1b[%d;%dH", editor.cursor_y, editor.cursor_x);
    write(STDOUT_FILENO, cursor_pos, strlen(cursor_pos));
}

int EditorReadKey() {
    int read_ret;
    char c;
   
    if ((read_ret = read(STDIN_FILENO, &c, 1)) == 0) return -1;

    if (read_ret == -1 && errno != EAGAIN) {
        ShowError("read");
    }
    if (c == ESC) {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return CURSOR_UP;
                case 'B': return CURSOR_DOWN;
                case 'C': return CURSOR_RIGHT;
                case 'D': return CURSOR_LEFT;
            }
        }
        return ESC;
    }
    return c;
}

void EditorProccessKey() {
    int key = EditorReadKey();

    // no key was read
    if (key == -1) return;

    // Exit the program when entering CTRL + q
    if (key == CTRL_KEY('q')) {

        // clear the screen before exiting
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);

        exit(0);
    }

    if (key == CURSOR_UP) editor.cursor_y--;
    else if (key == CURSOR_DOWN) editor.cursor_y++;
    else if (key == CURSOR_LEFT) editor.cursor_x--;
    else if (key == CURSOR_RIGHT) editor.cursor_x++;

}

int main(int argc, char** argv) {
    GetWindowSize(&editor.window_rows, &editor.window_cols);
    EditorInit();
    EnableRawMode();
    BufferInit();
    if (argc > 1) {
        ReadFileToBuffer(argv[1]);
    }
    while (1) {
        GetWindowSize(&editor.window_rows, &editor.window_cols);
        EditorClearScreen();
        EditorProccessKey();
    }
    BufferDestroy();
    EditorDestroy();
}
