all: 
	gcc -Wall -Werror -pthread -o s-talk main.c list.o sender.c

clean:
	rm s-talk