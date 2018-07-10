#! /bin/bash

if [ "$1" = "qemu" ]
then 
   KERNEL_PATH=`pwd`/../../../qemu/build/linux-4.17.2/       
   make ARCH=i386 -C $KERNEL_PATH M=`pwd` $2
else
    KERNEL_PATH=`pwd`/../../../bb_soft/build/linux/
    docker run -v "$(pwd):/home/user/linux_camp_modules" -v "$KERNEL_PATH:/home/user/linux_camp" build-bbb-eabi \
           "cd /home/user/linux_camp_modules && make -C /home/user/linux_camp  M=/home/user/linux_camp_modules $2"
fi
