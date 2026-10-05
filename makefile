build:
	gcc -Wall -Wextra tema2.c -o search_index

run:
	./search_index

clean:
	rm -f search_index

.PHONY: build run clean