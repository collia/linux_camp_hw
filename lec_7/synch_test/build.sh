#! /bin/bash

if [ "$1" = "qemu" ]
then
    QEMU_PATH=`pwd`/../../../qemo/build/
    KERNEL_PATH=${QEMU_PATH}linux-4.17.2/
    make ARCH=i386 -C $KERNEL_PATH M=`pwd` $2
    if [ $? == 0 ]
    then
        if [ "$2" != "clean" ]
        then
            cp *.ko $QEMU_PATH
            $QEMU_PATH/../run.sh
        fi
    fi
else
    KERNEL_PATH=`pwd`/../../../bb_soft/build/linux/
    docker run -v "$(pwd):/home/user/linux_camp_modules" -v "$KERNEL_PATH:/home/user/linux_camp" build-bbb-eabi \
           "cd /home/user/linux_camp_modules && make -C /home/user/linux_camp  M=/home/user/linux_camp_modules $2"
fi
