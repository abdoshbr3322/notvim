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

#define BOLD_ON  "\e[1m"


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
    CURSOR_LEFT = 500,
    CURSOR_RIGHT ,
    CURSOR_UP,
    CURSOR_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END,
    DELETE
};


// helpers
int IsPrintableCharacter(char c) {
    return (c >= 32 && c < 127);
}

int IsMoveKeyNormal(int key) {
    switch (key)
    {
    case 'k':
    case 'j':
    case 'l':
    case 'h':
    case CURSOR_UP:
    case CURSOR_DOWN:
    case CURSOR_RIGHT:
    case CURSOR_LEFT:
    case PAGE_UP:
    case PAGE_DOWN:
    case HOME:
    case END:
        return 1;
    default:
        return 0;
    }
}

int IsMoveKey(int key) {
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
        return 1;
    default:
        return 0;
    }
}


// TODO : Fix ShowError function
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
    String* string = malloc(sizeof(String));
    if (string == NULL) {
        ShowError("Memory couldn't be allocated");
    }

    // init
    string->size = 0;
    string->capacity = 10;
    string->str = malloc(string->capacity);    

    // Exit the program with error message if memory wasn't allocated
    if (string->str == NULL) {
        // free(string);
        ShowError("Memory couldn't be allocated");
    }

    // insert string terminator
    string->str[string->size] = 0;

    return string;
}

void StringAppend(String *string, const char* add) {
    size_t add_len = strlen(add);
    if (string->capacity <= (string->size + add_len)) {
        
        string->capacity = (string->size + add_len) * 2;
        string->str = realloc(string->str ,string->capacity);
    
        // Exit the program with error message if memory wasn't allocated
        if (string->str == NULL) {
            ShowError("Memory couldn't be allocated");
        }
    }

    memcpy(&string->str[string->size], add, add_len);
    string->size += add_len;
    string->str[string->size] = 0;
}

// void StringInsert(String* string, size_t pos, const char* add) {
//     if (pos > string->size) {
//         return;
//     } else if (pos == string->size) {
//         StringAppend(string, add);
//         return;
//     }

//     size_t add_len = strlen(add);
//     if (string->capacity < (string->size + add_len)) {
        
//         string->capacity = (string->size + add_len) * 2;
//         string->str = realloc(string->str ,string->capacity);
    
//         // Exit the program with error message if memory wasn't allocated
//         if (string->str == NULL) {
//             ShowError("Memory couldn't be allocated");
//         }
//     }

//     // move the current string after pos to the right
//     for (size_t i = string->size - 1; i >= pos; i--) {
//         string->str[i+add_len] = string->str[i];
//     }

//     memcpy(&string->str[pos], add, add_len);
//     string->size += add_len;
//     string->str[string->size] = 0;
// }

void StringInsertChar(String* string, int pos, const char add) {
    if (pos > (int)string->size) {
        // TODO : Show Error
        return;
    }

    if (string->capacity + 1 <= string->size) {
        
        string->capacity = (string->size + 1) * 2;
        string->str = realloc(string->str ,string->capacity);
    
        // Exit the program with error message if memory wasn't allocated
        if (string->str == NULL) {
            free(string);
            ShowError("Memory couldn't be allocated");
        }
    }

    // shift chars right
    for (int i = string->size - 1; i >= pos; i--) {
        string->str[i+1] = string->str[i];
    }

    string->str[pos] = add;
    string->size++;

    // add nullptr
    string->str[string->size] = 0;
}

void StringDeleteChar(String* string, int pos) {
    if (pos >= (int)string->size) {
        // TODO : Show Error
        return;
    }

    // shift chars left
    for (int i = pos; i < (int)string->size - 1; i++) {
        string->str[i] = string->str[i + 1];
    }
    string->size--;
    string->str[string->size] = 0;

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

// Editor Modes
enum MODE {
    NORMAL = 0,
    INSERT,
    COMMAND_LINE,
    VISUAL
};

// Editor Configuration
typedef struct
{
    struct termios default_term;
    enum MODE mode;
    size_t window_rows, window_cols;
    int cursor_x, cursor_y;
    int file_opened;
    int buffer_modified;
    String* file_name;
    int start_line, end_line;
    int cur_line, cur_column;
    int max_column;
} Editor;

Editor editor;

void EditorInit() {
    if (tcgetattr(STDIN_FILENO, &editor.default_term) == -1) {
        ShowError("tcgetattr");
    }
    editor.mode = NORMAL;
    GetWindowSize(&editor.window_rows, &editor.window_cols);
    editor.cursor_x = editor.cursor_y = 1;
    editor.file_opened = 0;
    editor.buffer_modified = 0;
    editor.file_name = StringInit();
    editor.start_line = editor.end_line = 0;
    editor.cur_line = editor.cur_column = 0;
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

void BufferSplitLine(int idx_row, int idx_col) {
    if (array_buffer.size == array_buffer.capacity) 
        BufferExpandCapacity();
    
    // Shift right rows
    for (int i = array_buffer.size - 1; i > idx_row; i--) {
        array_buffer.array[i+1] = array_buffer.array[i];
    }
    array_buffer.size++;

    String* cur_line = array_buffer.array[idx_row];
    
    // insert new line
    String* new_line = StringInit();
    StringAppend(new_line, &cur_line->str[idx_col]);
    array_buffer.array[idx_row+1] = new_line;

    // resize current line
    cur_line->size = idx_col;
    cur_line->str[cur_line->size] = 0;
}

void BufferMergeLines(int idx_row) {
    if (idx_row == 0) {
        // TODO : Show Error
        return;
    }

    String *cur_line = array_buffer.array[idx_row-1], 
           *next_line = array_buffer.array[idx_row];

    // Shift left lines
    for (size_t i = idx_row; i + 1 < array_buffer.size; i++) {
        array_buffer.array[i] = array_buffer.array[i+1];
    }
    array_buffer.size--;


    StringAppend(cur_line, next_line->str);

    StringDestroy(next_line);
}

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
    const char* tilde_cursor_pos = "\x1b[H";
    String* tildes = StringInit();
    StringAppend(tildes, tilde_cursor_pos);

    // color the tildes blue
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
    String* message = StringInit();

    char message_pos[20];
    snprintf(message_pos, sizeof(message_pos), "\x1b[%zu;%dH", editor.window_rows, 2);

    StringAppend(message, message_pos);
    StringAppend(message ,"\x1b[2K");

    StringAppend(message, CYAN);
    StringAppend(message, BOLD_ON);
    switch (editor.mode)
    {
    case NORMAL:
        if (editor.file_name->size < 40) {
            StringAppend(message, editor.file_name->str);
        }
        break;
    case INSERT:
        StringAppend(message, "-- INSERT MODE --");
        break;
    case VISUAL:
        StringAppend(message, "-- VIUSAL MODE --");
        break;
    case COMMAND_LINE:
        StringAppend(message, ":");
        break;
    default:
        break;
    }
    StringAppend(message, COLOR_RESET);
    write(STDOUT_FILENO, message->str, message->size);

    char status[40];
    snprintf(status, sizeof(status), "%d,%d", editor.cur_line + 1, editor.cur_column + 1);
    
    char status_pos[20];
    snprintf(status_pos, sizeof(status_pos), "\x1b[%zu;%zuH", editor.window_rows, editor.window_cols - 3 - strlen(status));

    StringAppend(status_buffer, status_pos);
    StringAppend(status_buffer, status);

    write(STDOUT_FILENO, status_buffer->str, status_buffer->size);

    StringDestroy(message);
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
    if (!editor.file_opened && !editor.buffer_modified) {
        ShowWelcomeMessage();
    }
    ShowTextFromBuffer();
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
            if (seq[1] >= '1' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return ESC;

                if (seq[2] == '~' && seq[1] == '5') return PAGE_UP;
                else if (seq[2] == '~' && seq[1] == '6') return PAGE_DOWN;
                else if (seq[2] == '~' && seq[1] == '3') return DELETE;
                else return ESC;
            }
        }
        return ESC;
    }
    return c;
}

void CalculateCursorX() {
    editor.cursor_x = (editor.cur_column % editor.window_cols) + 1;
}

void CalculateCursorY() {
    editor.cursor_y = 1;
    for (int line = editor.start_line; line < editor.cur_line; line++) {
        String* cur_line = array_buffer.array[line];
        // number of lines needed to render a line
        int needed = (cur_line->size ? ceil_d(cur_line->size, editor.window_cols) : 1);
        
        editor.cursor_y += needed;
    }
    editor.cursor_y += (editor.cur_column / editor.window_cols);

}

void ScrollUp() {
    if (editor.start_line > 0 && editor.cursor_y <= 5) { // scroll up
        editor.cur_line--;
        editor.start_line--;
        editor.end_line--;
    } else {
        editor.cur_line--;
    }
}

void ScrollDown() {
    if (editor.end_line < ((int)array_buffer.size - 1) &&  editor.cursor_y >= ((int)editor.window_rows - 6)) { // scroll down
        editor.cur_line++;
        editor.start_line++;
        editor.end_line++;
    } else {
        editor.cur_line++;
    }
}

void MoveCursorAndScroll(int move) {
    if (array_buffer.size == 0) return;
    String* cur_line = array_buffer.array[editor.cur_line];

    switch (move)
    {
    case CURSOR_UP:
    case 'k':
        if (editor.cur_line > 0) {
            ScrollUp();
            cur_line = array_buffer.array[editor.cur_line];
        }
        break;
    case CURSOR_DOWN:
    case 'j':
        if (editor.cur_line < ((int)array_buffer.size - 1)) {
            ScrollDown();
            cur_line = array_buffer.array[editor.cur_line];
        }
        break;
    case CURSOR_RIGHT:
    case 'l':
        if (editor.cur_column < (int)cur_line->size)
            editor.max_column++;
        break;
    case CURSOR_LEFT:
    case 'h':
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
        editor.max_column = 0;
        break;
    case END:
        editor.max_column = cur_line->size;
        break;
    default:
        ShowError("Not a valid move");
        break;
    }
    editor.cur_column = min(editor.max_column, cur_line->size);

    CalculateCursorX();
    CalculateCursorY();

}

// Key proccessing for differnet modes
void InsertProccessKey(int key) {
    // New line
    if (key == '\r') {
        BufferSplitLine(editor.cur_line, editor.cur_column);
        MoveCursorAndScroll(CURSOR_DOWN);
        MoveCursorAndScroll(HOME);
    }

    // Backspace -> Delete backward
    if (key == 127) {
        String* cur_line = array_buffer.array[editor.cur_line];
        if (editor.cur_column > 0) { // Delete a char
            StringDeleteChar(cur_line, editor.cur_column - 1);
            MoveCursorAndScroll(CURSOR_LEFT);
        } 
        
        else if (editor.cur_line > 0) { // Delete a line
            // get prev line size to move the cursor after it
            String* prev_line = array_buffer.array[editor.cur_line - 1];
            size_t prev_size = prev_line->size;

            BufferMergeLines(editor.cur_line);
            editor.cur_column = editor.max_column = prev_size;
            ScrollUp();
            CalculateCursorX();
            CalculateCursorY();
        }
        return;
    } 

    // Delete key : Delete forward
    if (key == DELETE) {
        String* cur_line = array_buffer.array[editor.cur_line];
        if (editor.cur_column < (int)cur_line->size) {
            StringDeleteChar(cur_line, editor.cur_column);
        } else if (editor.cur_line < (int)array_buffer.size - 1) {
            BufferMergeLines(editor.cur_line + 1);
        }
    }
    // Move key
    if (IsMoveKey(key)) {
        MoveCursorAndScroll(key);
    }
    
    // Printable
    if (IsPrintableCharacter(key)) {
        editor.buffer_modified = 1;
        String* cur_line = array_buffer.array[editor.cur_line];
        StringInsertChar(cur_line, editor.cur_column, key);
        MoveCursorAndScroll(CURSOR_RIGHT);
    }
}

void NormalProccessKey(int key) {
    if (IsMoveKeyNormal(key)) {
        MoveCursorAndScroll(key);
    }

    switch (key)
    {
    case 'i':
        editor.mode = INSERT;
        break;
    case 'v':
        editor.mode = VISUAL;
        break;
    case ':':
        editor.mode = COMMAND_LINE;
        break;
    default:
        break;
    }
}

void CommandProccessKey(int key) {

}

void VisualProccessKey(int key) {

}
void EditorProccessKey() {
    int key = EditorReadKey();

    // no key was read
    if (key == -1) return;

    // Exit the program when entering CTRL + q
    if (key == CTRL_KEY('q')) {
        exit(0);
    }

    // reset to normal mode when pressing Escape
    if (key == ESC) {
        editor.mode = NORMAL;
    }

    switch (editor.mode)
    {
    case NORMAL:
        NormalProccessKey(key);
        break;
    case INSERT:
        InsertProccessKey(key);
        break;
    case COMMAND_LINE:
        CommandProccessKey(key);
        break;
    case VISUAL:
        VisualProccessKey(key);
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
    } else {
        BufferAppendLine("");
    }
    while (1) {
        GetWindowSize(&editor.window_rows, &editor.window_cols);
        EditorClearScreen();
        EditorProccessKey();
    }
    
}
