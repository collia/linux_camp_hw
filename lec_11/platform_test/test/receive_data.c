#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef QEMU
#define MEM_BASE 0x20000800
#define REG_BASE 0x20001000
#else
#define MEM_BASE 0x9f201000
#define REG_BASE  0x9f202000
#endif
#define WRITE_MEM_OFFSET 0x800

#define MEM_SIZE	(1024)
#define REG_SIZE	(8*2)

#define PLAT_IO_FLAG_REG		(8) /*Offset of flag register*/
#define PLAT_IO_SIZE_REG		(0xc) /*Offset of flag register*/
#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */


extern int errno;

int main(int argc, char **argv)
{
	volatile unsigned int *reg_addr = NULL, *count_addr, *flag_addr;
	volatile unsigned char *mem_addr = NULL;
	unsigned int i, num = 0, val;

	int fd = open("/dev/mem",O_RDWR|O_SYNC);
	if(fd < 0)
	{
		printf("Can't open /dev/mem\n");
		return -1;
	}
	mem_addr = (unsigned char *) mmap(0, MEM_SIZE, PROT_READ, MAP_SHARED, fd, MEM_BASE);
	if(mem_addr == MAP_FAILED)
	{
		printf("Can't mmap memory\n");
        perror("mmap");
		return -1;
	}

	reg_addr = (unsigned int *) mmap(0, REG_SIZE, PROT_WRITE |PROT_READ, MAP_SHARED, fd, REG_BASE);
    if(mem_addr == MAP_FAILED)
	{
		printf("Can't mmap registers\n");
        perror("mmap regs");
		return -1;
	}


	flag_addr = reg_addr + (PLAT_IO_FLAG_REG / sizeof(*reg_addr));
	count_addr = reg_addr + (PLAT_IO_SIZE_REG / sizeof(*reg_addr));


    if((*flag_addr) & PLAT_IO_DATA_READY)
    {
            num = *count_addr;
            if(num > MEM_SIZE)
                    num = MEM_SIZE;
            printf("Reading %d bytes %x\n", num, *flag_addr);
            for (i=0; i< num; i++) {
                    printf("data[%d] = %hx \n", i, mem_addr[i+WRITE_MEM_OFFSET]);

            }
            *flag_addr &= ~PLAT_IO_DATA_READY;
            *count_addr = 0;
    }
    else
    {
            printf("Data is not ready\n");
    }
	return 0;
}
