CXX = g++ -g -fPIC -pthread
CC= gcc -fPIC
NETLIBS= -lnsl

all: myhttpd daytime-server use-dlopen hello.so jj-mod.so

daytime-server : daytime-server.o
	$(CXX) -o $@ $@.o $(NETLIBS)

myhttpd : myhttpd.o
	$(CXX) -o $@ $@.o $(NETLIBS) -ldl

use-dlopen: use-dlopen.o
	$(CXX) -o $@ $@.o $(NETLIBS) -ldl

hello.so: hello.o
	ld -G -o hello.so hello.o

jj-mod.so: jj-mod.o util.o 
	ld -G -o jj-mod.so jj-mod.o util.o

jj-mod.o: jj-mod.c
	$(CC) -g -c jj-mod.c

util.o: util.c
	$(CC) -c util.c



%.o: %.cc
	@echo 'Building $@ from $<'
	$(CXX) -o $@ -c -I. $<

# .PHONY: git-commit
# git-commit:
# 	git checkout
# 	git add *.cc *.h Makefile >> .local.git.out  || echo
# 	git commit -a -m 'Commit' >> .local.git.out || echo
# 	git push origin master 

.PHONY: clean
clean:
	rm -f *.o use-dlopen hello.so jj-mod.so
	rm -f *.o daytime-server myhttpd

