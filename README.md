# notvim


A lightweight, vim-like text editor written in C.

## Features

- Simple and intuitive interface
- Basic text editing capabilities
- Written in pure C with minimal dependencies

## Installation

### Prerequisites

- GCC compiler
- Make
- Linux/Unix-based system

### Building from source

1. Clone the repository:
```bash
git clone https://github.com/abdoshbr3322/notvim.git
cd notvim
```

2. Compile the source:
```bash
make
```

3. Run the editor:
```bash
./notvim [filename]
```

4. Show help file:
```bash
./notvim -h
```

## Todo List

- [x] Enter raw mode and get input

- [x] Draw Tildes and Show Welcome Message

- [x] Implement data structures for buffer

- [ ] Implement text viewer
  - [x] File reading
  - [x] Content display
  - [x] Scrolling 
  - [X] Improve Scrolling 
  - [ ] Syntax Highlihgting

- [X] Writing to files
  - [X] Save commands
  - [X] Save buffer contents
  
- [X] Implement different modes
  - [X] Normal mode
  - [X] Insert mode
  - [X] Visual mode
  - [X] Command Line mode
