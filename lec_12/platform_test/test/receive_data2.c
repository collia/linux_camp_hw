#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>



#define MEM_BASE1 0x9f201000
#define REG_BASE1  0x9f202000

#define MEM_BASE2 0x9f211000
#define REG_BASE2 0x9f212000

#define MEM_SIZE	0x1000
#define REG_SIZE	8

#define PLAT_WRITE_FLAG_REG		(8) /*Offset of flag register*/
#define PLAT_WRITE_SIZE_REG		(0xc) /*Offset of flag register*/
#define PLAT_IO_DATA_READY	(1) /*IO data ready flag */

#define MAX_DEVICES	2

extern int errno;

struct my_device {
	uint32_t mem_base;
	uint32_t mem_size;
	uint32_t reg_base;
	uint32_t reg_size;	
};

static struct my_device my_devices[MAX_DEVICES] = {{
	.mem_base = MEM_BASE1,
	.mem_size = MEM_SIZE,
	.reg_base = REG_BASE1,
	.reg_size = REG_SIZE,
	},
	{
	.mem_base = MEM_BASE2,
	.mem_size = MEM_SIZE,
	.reg_base = REG_BASE2,
	.reg_size = REG_SIZE,
	},
};
int usage(char **argv)
{
	printf("Program sends file to the specific device\n");
	printf("Usage: %s <device>\n", argv[0]);
	return -1;
}

int main(int argc, char **argv)
{
	volatile unsigned int *reg_addr = NULL, *count_addr, *flag_addr;
	volatile unsigned char *mem_addr = NULL;
	unsigned int i, device, ret, len, count;
	struct stat st;

	if (argc != 2) {
		return usage(argv);
	}

	device = atoi(argv[1]);
	if (device >= MAX_DEVICES)
		return usage(argv);

	int fd = open("/dev/mem",O_RDWR|O_SYNC);
	if(fd < 0)
	{
		printf("Can't open /dev/mem\n");
		return -1;
	}
	mem_addr = (unsigned char *) mmap(0, my_devices[device].mem_size,
				PROT_WRITE, MAP_SHARED, fd, my_devices[device].mem_base);
	if(mem_addr == (void*)(-1))
	{
		printf("Can't mmap\n");
		return -1;
	}

	reg_addr = (unsigned int *) mmap(0, my_devices[device].reg_size,
			PROT_WRITE | PROT_READ, MAP_SHARED, fd, my_devices[device].reg_base);
	if(reg_addr == (void*)(-1))
	{
		printf("Can't mmap\n");
		return -1;
	}
	flag_addr = &reg_addr[PLAT_WRITE_FLAG_REG/sizeof(*reg_addr)];
	count_addr = &reg_addr[PLAT_WRITE_SIZE_REG/sizeof(*reg_addr)];
    printf("%x %x %x %x\n", reg_addr[0], reg_addr[1], reg_addr[2], reg_addr[3]);
    
    count = *count_addr;
    if( *flag_addr & PLAT_IO_DATA_READY)
    {
            for(i = 0; i < count; i++)
            {
                    if((i%16 == 0))
                    {
                            printf("\n %#04x: ");
                    } else if((i%8 == 0) && (i != 0)) {
                            printf("  ");
                    }
                    printf("%02X ", mem_addr[i]);
                    
            }
            printf("\n");
       
    }
    else
    {
            printf("Data is not ready\n");
    }
    *flag_addr = 0;
	return 0;
}
