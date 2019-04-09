#
# Author: Yash Gupta <yash_gupta12@live.com>
#

CC = gcc
CFLAGS = -O0
EXEC = function_test paging_test

all: function_test paging_test

function_test: function_test.c
	$(CC) -o function_test -g $(CFLAGS) function_test.c

paging_test: paging_test.c
	$(CC) -o paging_test -g $(CFLAGS) paging_test.c

clean:
	rm -rf $(EXEC) *.dSYM