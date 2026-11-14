# Logic tests for the module against stub wlroots headers (NOT a build against
# the real wlroots/Mango; see README "Testing").
CC ?= gcc
CFLAGS = -std=gnu11 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
         -Istubs -I../include
LDLIBS = -lm
all: shake-test
shake-test: shake_test.c ../src/input/shake_to_locate.c stubs/stubs.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -o $@ $^ $(LDLIBS)
check: shake-test
	./shake-test
clean:
	rm -f shake-test scaler-test

scaler-test: scaler_test.c stubs/stubs.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -Wno-misleading-indentation -o $@ $^ $(LDLIBS)
check-all: check scaler-test
	./scaler-test
