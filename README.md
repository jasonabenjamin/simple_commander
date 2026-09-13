# Simple Commander

Simple Commander is a lightweight, DOS-era-inspired file manager written in C++17 using SDL2 and SDL_ttf.

It is based primarily on **Directory Commander (DCOM)**, with influences from other classic file managers such as **Midnight Commander**. The goal is to preserve the straightforward, keyboard-oriented feel of classic DOS file managers while providing a modern SDL2 window with mouse support and resizing.

## Features

- Classic single-directory file-manager interface
- Keyboard and mouse operation
- Column-major file listing
- Drives and directory Tree views
- Copy, Move, Delete, Rename, and Mkdir
- File marking and Select All / Deselect
- Filename filtering
- Built-in read-only file viewer with word wrapping
- Built-in text editor
- Global horizontal scrolling in the editor
- Hidden-file display
- Multiple color schemes
- Windows executable launching, including console programs
- Resizable SDL2 interface

### Editor Commands

- **Alt-D** — Delete the current line
- **Alt-I** — Insert a blank line before the current line
- **Alt-W** — Write/save the current file
- **Esc** — Exit the editor

## License

Simple Commander is released under the **AI Freedom License (AIFL) 1.0**.

See `AIFL-1.0.txt` for the license and `SCOMHELP.TXT` for the complete program help reference.

## Downloads

A prebuilt Windows release is provided for users who do not want to compile the program. The Windows ZIP contains the executable and required runtime DLLs so that it can be extracted and run without setting up a development environment.

The source distribution is available separately for users who want to build, study, or modify the program.

## Building from Source

The source is written in C++17 and uses SDL2 and SDL_ttf. A Makefile is included for the intended MinGW/MSYS2 build environment.

## Background

Simple Commander is an independent project inspired by classic DOS file managers. Its primary inspiration is Directory Commander, while features and ideas from other file managers, including Midnight Commander, have also influenced its development.

It is not intended to be a direct copy of DCOM. It is an independent implementation that combines the classic command-oriented file-manager approach with SDL2.

## Help

`SCOMHELP.TXT` contains the program's detailed help reference, including navigation, commands, viewer controls, editor controls, and other operating information.

## Status

Simple Commander is under development. Features and behavior may change as the project evolves.
