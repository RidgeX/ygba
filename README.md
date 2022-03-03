# ygba

Yet another Game Boy Advance emulator

![Screenshot](img/screenshot.png)

## Building

See BUILDING.md for information on how to set up dependencies and environment variables. ygba can then be compiled from the command line using the included build scripts. On Windows, MSYS2 (with MinGW-w64 gcc) is recommended for the best performance.

## Usage

A GBA BIOS file is required to run ygba. You can [dump your own with a flashcart](https://github.com/mgba-emu/bios-dump) or use [Normmatt's open-source replacement](https://github.com/Nebuleon/ReGBA/tree/master/bios). To load ROM files drag and drop them onto the executable or onto the emulator window.

## Controls

| Button | Key                  |
|--------|----------------------|
| Up     | <kbd>&uarr;</kbd>    |
| Down   | <kbd>&darr;</kbd>    |
| Left   | <kbd>&larr;</kbd>    |
| Right  | <kbd>&rarr;</kbd>    |
| A      | <kbd>X</kbd>         |
| B      | <kbd>Z</kbd>         |
| L      | <kbd>A</kbd>         |
| R      | <kbd>S</kbd>         |
| Start  | <kbd>Enter</kbd>     |
| Select | <kbd>Backspace</kbd> |
