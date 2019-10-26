PROGRAMS = clean main

all: $(PROGRAMS)

main:
	gcc main.c -o main

.PHONY: clean
clean:
	@rm -f $(PROGRAMS) *.o core
