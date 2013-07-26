anthy-dic-cli: anthy-dic-cli.c
	gcc -g -O2 -Wall -o anthy-dic-cli anthy-dic-cli.c $(shell pkg-config --cflags --libs anthy)

clean:
	-rm anthy-dic-cli
