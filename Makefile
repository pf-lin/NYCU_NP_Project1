all:
	g++ -o npshell -Wall -g -fsanitize=address -static-libasan npshell.cpp
	mkdir -p bin
	cp /bin/ls /bin/cat bin/
	make -C commands

clean:
	rm -rf npshell
	rm -rf bin