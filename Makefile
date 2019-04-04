#
# Author: Yash Gupta <yash_gupta12@live.com>
#

CC = gcc
CFLAGS = -O0
EXEC = test

$(EXEC): unit_tests.c
	$(CC) -o $(EXEC) -g $(CFLAGS) unit_tests.c

clean:
	rm -rf $(EXEC) $(EXEC)*