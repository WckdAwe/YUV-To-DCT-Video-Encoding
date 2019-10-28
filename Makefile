PROGRAMS = clean main

all: $(PROGRAMS)

main:
	gcc main.c -o main -lm

.PHONY: clean
clean:
	@rm -f $(PROGRAMS) *.o core
