build:
	gcc ./main.c -g -lSDL2 -lGL
 
run: build
	./a.out

debug: build
	gdb ./a.out
format:
	clang-format ./*.c -i
