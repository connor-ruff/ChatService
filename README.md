# ProgAssign3
- Connor Ruff - cruff
- Kelly Buchanan - kbuchana
- Ryan Wigglesworth - rwiggles

# Files
 - Directory: client/
	- client.cpp: c++ code for the chat client
	- Makefile:   to build the executable for chat client
- Directory: server/
	- server.cpp: c++ code for the chat server
	- users.csv:  list of known users for the server (server updates this file as new users register)	
	- Makefile: to build the executable for chat server

# Instructions
- To run server:
	cd server/
	make server
	./chatserver <port>

- To run client
	cd client/
	make client
	./client student<XX>.nd.edu <port> <username>


