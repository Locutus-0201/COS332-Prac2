Server: Server.cpp
	g++ Server.cpp -o Server -pthread

deploy: Server
	sudo ./Server

clean:
	rm -f Server