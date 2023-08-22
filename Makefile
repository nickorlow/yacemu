build:
	gcc ./main.c
 
run: build
	./a.out

format:
	clang-format ./*.c -i
