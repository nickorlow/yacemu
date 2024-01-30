# yacemu (Yet Another Chip Eight Emulator)
A Chip-8 emulator (interpereter) written in C. Depends on SDL2.

### Building & Testing 

In order to run yacemu, you must have sdl2 and make installed.

Once you have the dependencies installed, you can build the emulator with:
```shell
make build
```

You can build start the emulator with:
```shell
make run ROM_FILE=[PATH_TO_YOUR_ROM]
```

If you would like to run it in turbo mode (faster emulation, useful for testing) then run:
```shell
make run-turbo ROM_FILE=[PATH_TO_YOUR_ROM]
```

If you would like to debug with gdb then run:
```shell
make debug ROM_FILE=[PATH_TO_YOUR_ROM]
```

If you would like to debug with gdb in turbo mode then run:
```shell
make debug-turbo ROM_FILE=[PATH_TO_YOUR_ROM]
```

### Running

If you have a binary, you can run it with the following:

```shell
yacemu [PATH_TO_YOUR_ROM]
```

If you would like to run it in turbo mode (faster emulation, useful for testing) then run:

```shell
yacemu [PATH_TO_YOUR_ROM] turbo
```

### Todo
- [x] Graphics
- [x] Corax+ Required Instructions
- [x] Proper Flag Handling 
- [x] Working Input
- [x] More Instructions
- [x] Tetris Working Running
- 1/2 Pass Quirks Test (DispQuirk is a bit touchy) 
- [x] Add beeper (instead of sound, screen becomes red)
- [ ] Code cleanup
- [ ] Rest of Instructions (I believe a few are missing)

### Screenshots

![Chip 8 Logo Demo](https://github.com/nickorlow/yacemu/blob/main/screenshots/chip8-logo.png?raw=true)
![IBM Logo Demo](https://github.com/nickorlow/yacemu/blob/main/screenshots/ibm-logo.png?raw=true)
![Corax Plus Test Demo](https://github.com/nickorlow/yacemu/blob/main/screenshots/corax+-test.png?raw=true)
![Flag Test Demo](https://github.com/nickorlow/yacemu/blob/main/screenshots/flag-test.png?raw=true)
![Quirks Test](https://github.com/nickorlow/yacemu/blob/main/screenshots/quirks-test.png?raw=true)
