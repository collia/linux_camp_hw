#! /bin/bash

if [ "$1" = "qemu" ]
then
    QEMU_PATH=`pwd`/../../../qemo/build/
    KERNEL_PATH=${QEMU_PATH}linux-4.17.2/
    make ARCH=i386 -C $KERNEL_PATH M=`pwd` $2
    if [ $? == 0 ]
    then
        if [ "$2" != "clean" -a "$2" != "debug" ]
        then
            cp *.ko $QEMU_PATH
            $QEMU_PATH/../run.sh
        elif [ "$2" != "debug" ]
             cp *.ko $QEMU_PATH
            $QEMU_PATH/../rundebug.sh
        fi
    fi
else
    KERNEL_PATH=`pwd`/../../../bb_soft/build/linux/
    docker run --rm=true -v "$(pwd):/home/user/linux_camp_modules" -v "$KERNEL_PATH:/home/user/linux_camp" build-bbb-eabi \
           "cd /home/user/linux_camp_modules && make -C /home/user/linux_camp  M=/home/user/linux_camp_modules C=1 $2"
fi
