
CC=$(CROSS_COMPILE)gcc

ifeq ("$(CROSS_COMPILE)", "")
CFLAGS=-I. -I.. -m32 -march=i386 -DQEMU
else
CFLAGS=-I../linux_camp_modules
endif

.PHONY: all
all: send_data2 receive_data2 send_ioctl


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

send_data2: send_data2.o
	$(CC) -o $@ $< $(CFLAGS)

receive_data2: receive_data2.o
	$(CC) -o $@ $< $(CFLAGS)

send_ioctl: send_ioctl.o
	$(CC) -o $@ $< $(CFLAGS)


clean:
	rm -f *.o
	rm -f hello
