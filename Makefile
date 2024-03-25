all:
	g++ -o npshell npshell.cpp
	mkdir -p bin
	cp /bin/ls /bin/cat bin/
	make -C commands

clean:
	rm -rf npshell
	rm -rf bin