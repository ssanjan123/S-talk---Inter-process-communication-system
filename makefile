all: 
	gcc -Wall -Werror -pthread -o s-talk main.c list.o 

clean:
	rm s-talk