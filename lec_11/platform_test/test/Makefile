
CC=$(CROSS_COMPILE)gcc

ifeq ("$(CROSS_COMPILE)", "")
CFLAGS=-I. -m32 -march=i386 -DQEMU
endif

.PHONY: all
all: send_data receive_data


%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

send_data: send_data.o
	$(CC) -o $@ $< $(CFLAGS)

send_data2: send_data2.o
	$(CC) -o $@ $< $(CFLAGS)

receive_data: receive_data.o
	$(CC) -o $@ $< $(CFLAGS)


clean:
	rm -f *.o
	rm -f hello
