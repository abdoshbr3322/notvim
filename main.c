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

int min(int a, int b) {
    if (a < b) return a;
    return b;
}

int max(int a, int b) {
    if (a > b) return a;
    return b;
}

// define keys enum
enum KEYS {
    CURSOR_LEFT = 'h',
    CURSOR_RIGHT = 'l',
    CURSOR_UP = 'k',
    CURSOR_DOWN = 'j',
    PAGE_UP = 1000,
    PAGE_DOWN,
    HOME,
    END

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

void GetWindowSize(size_t* window_rows, size_t* window_cols) {
    struct winsize window;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == -1 || window.ws_row == 0) {
        ShowError("Failed to get window size");  
    } else {
        *window_rows = window.ws_row;
        *window_cols = window.ws_col;
    }
}

// Editor Configuration
typedef struct
{
    struct termios default_term;
    size_t window_rows, window_cols;
    int cursor_x, cursor_y;
    int file_opened;
    String* file_name;
    int start_line, end_line;
    int cur_row, cur_column;
    int max_column;
} Editor;

Editor editor;

void EditorInit() {
    if (tcgetattr(STDIN_FILENO, &editor.default_term) == -1) {
        ShowError("tcgetattr");
    }
    GetWindowSize(&editor.window_rows, &editor.window_cols);
    editor.cursor_x = editor.cursor_y = 1;
    editor.file_opened = 0;
    editor.file_name = StringInit();
    editor.start_line = editor.end_line = 0;
    editor.cur_row = editor.cur_column = 0;
    editor.max_column = 0;
}

void EditorDestroy() {
    StringDestroy(editor.file_name);
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

void DisableRawMode () {
    tcsetattr(STDIN_FILENO, TCIFLUSH, &editor.default_term);

    // reset the original screen buffer
    write(STDOUT_FILENO ,"\x1b[?1049l", 8);
    fflush(stdout);
}

void EnableRawMode () {
    atexit(DisableRawMode);
    
    struct termios raw = editor.default_term;

    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    // Switch to another screen buffer
    write(STDOUT_FILENO ,"\x1b[?1049h", 8);

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

    for (size_t i = 0; i < editor.window_rows - 1; i++) {
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

void StatusBar() {
    String *status_buffer = StringInit();

    char status[40];
    snprintf(status, sizeof(status), "%d,%d", editor.cur_row + 1, editor.cur_column + 1);
    
    char bar_pos[20];
    snprintf(bar_pos, sizeof(bar_pos), "\x1b[%zu;%zuH", editor.window_rows, editor.window_cols - 3 - strlen(status));

    StringAppend(status_buffer ,"\x1b[K");
    StringAppend(status_buffer, bar_pos);
    StringAppend(status_buffer, status);

    write(STDOUT_FILENO, status_buffer->str, status_buffer->size);

    StringDestroy(status_buffer);
}

void ShowWelcomeMessage() {
    const char* message = "~ Welcome To notvim. Made with <3 By Abdullah ~";
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
    for (size_t line = editor.start_line; line < array_buffer.size; line++) {
        String* cur_line = array_buffer.array[line];

        // calculate number of lines in the terminal needed to render the current line
        int needed = (cur_line->size ? ceil_d(cur_line->size, editor.window_cols) : 1);

        // stop rendering when the terminal is full
        lines_needed += needed;
        if (lines_needed >= (int)editor.window_rows) break;

        editor.end_line = line;
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

    if (fptr == NULL) { // The file doesn't exist
        BufferAppendLine("");
        return;
    }

    while (getline(&buffer, &len, fptr) != -1) {
        buffer[strlen(buffer) - 1] = '\0';
        BufferAppendLine(buffer);
    }
    if (array_buffer.size == 0) {
        BufferAppendLine("");
    }
    fclose(fptr);

}

void EditorClearScreen() {
    DrawTildes();
    StatusBar();
    if (editor.file_opened == 0) {
        ShowWelcomeMessage();
    } else {
        ShowTextFromBuffer();
    }
    char cursor_pos[20];
    snprintf(cursor_pos, sizeof(cursor_pos), "\x1b[%d;%dH", editor.cursor_y, editor.cursor_x);
    write(STDOUT_FILENO, cursor_pos, strlen(cursor_pos));
}


void MoveCursorAndScroll(enum KEYS move) {
    if (array_buffer.size == 0) return;
    switch (move)
    {
    case CURSOR_UP:
        if (editor.cur_row > 0) {
            if (editor.start_line > 0 && editor.cursor_y <= 5) { // scroll up
                editor.cur_row--;
                editor.start_line--;
                editor.end_line--;
            } else {
                editor.cur_row--;
                editor.cursor_y--;
            }
            editor.cur_column = min(array_buffer.array[editor.cur_row]->size, editor.cur_column);
        }
        break;
    case CURSOR_DOWN:
        if (editor.cur_row < ((int)array_buffer.size - 1)) {
            if (editor.end_line < ((int)array_buffer.size - 1) &&  editor.cursor_y >= ((int)editor.window_rows - 6)) { // scroll down
                editor.cur_row++;
                editor.start_line++;
                editor.end_line++;
            } else {
                editor.cur_row++;
                editor.cursor_y++;
            }
            editor.cur_column = min(array_buffer.array[editor.cur_row]->size, editor.cur_column);
        }
        break;
    case CURSOR_RIGHT:
        if (editor.cur_column < (int)array_buffer.array[editor.cur_row]->size)
            editor.max_column++;
        break;
    case CURSOR_LEFT:
        if (editor.cur_column > 0)
            editor.max_column--;
        break;
    case PAGE_UP:
        for (size_t i = 0; i < editor.window_rows; i++) {
            MoveCursorAndScroll(CURSOR_UP);
        }
        break;
    case PAGE_DOWN:
        for (size_t i = 0; i < editor.window_rows; i++) {
            MoveCursorAndScroll(CURSOR_DOWN);
        }
        break;
    case HOME:
        editor.cur_column = 0;
        break;
    case END:
        editor.cur_column = array_buffer.array[editor.cur_row]->size;
        break;
    default:
        ShowError("Not a valid move");
        break;
    }
    editor.cur_column = min(editor.max_column, array_buffer.array[editor.cur_row]->size);
    editor.cursor_x = editor.cur_column + 1;

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
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return ESC;

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return CURSOR_UP;
                case 'B': return CURSOR_DOWN;
                case 'C': return CURSOR_RIGHT;
                case 'D': return CURSOR_LEFT;
                case 'H': return HOME;
                case 'F': return END;
            }
            if (seq[1] == '5' || seq[1] == '6') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return ESC;

                if (seq[2] == '~' && seq[1] == '5') return PAGE_UP;
                else if (seq[2] == '~' && seq[1] == '6') return PAGE_DOWN;
                else return ESC;
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
        exit(0);
    }

    switch (key)
    {
    case CURSOR_UP:
    case CURSOR_DOWN:
    case CURSOR_RIGHT:
    case CURSOR_LEFT:
    case PAGE_UP:
    case PAGE_DOWN:
    case HOME:
    case END:
        MoveCursorAndScroll(key);
        break;
    
    default:
        break;
    }
}

void cleanup() {
    BufferDestroy();
    EditorDestroy();
    DisableRawMode();
}

int main(int argc, char** argv) {
    EditorInit();
    EnableRawMode();
    BufferInit();
    atexit(cleanup);
    if (argc > 1) {
        ReadFileToBuffer(argv[1]);
    }
    while (1) {
        GetWindowSize(&editor.window_rows, &editor.window_cols);
        EditorClearScreen();
        EditorProccessKey();
    }
    
}
