all: 
	gcc -Wall -Werror -pthread -o s-talk main.c list.o keyboard.c sender.c 

clean:
	rm s-talk