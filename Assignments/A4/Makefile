all: boardgen.c block.c coordinator.c
	gcc  -o block block.c
	gcc  -o coordinator coordinator.c

run: all
	./coordinator

clean:
	-rm -f block coordinator
