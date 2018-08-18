
CC=$(CROSS_COMPILE)gcc

ifeq ("$(CROSS_COMPILE)", "")
CFLAGS=-I. -I.. -m32 -march=i386 -DQEMU
else
CFLAGS=-I../linux_camp_modules
endif

.PHONY: all
all: mmap_test


%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

mmap_test: mmap_test.o
	$(CC) -o $@ $< $(CFLAGS)


clean:
	rm -f *.o
	rm -f mmap_test
