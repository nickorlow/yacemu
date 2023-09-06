build:
	gcc ./main.c -g -lSDL2 -lGL
 
run: build
	./a.out ${ROM_FILE}

run-turbo: build
	./a.out ${ROM_FILE} turbo

debug: build
	gdb ./a.out ${ROM_FILE}

debug-turbo: build
	gdb ./a.out ${ROM_FILE} turbo
	
format:
	clang-format ./*.c -i
