#! /bin/bash

TEST_TARGET=hw3-app

if [ "$1" = "qemu" ]
then
    QEMU_PATH=`pwd`/../../../qemu/build/
    KERNEL_PATH=${QEMU_PATH}linux-4.17.2/
    make ARCH=i386 -C $KERNEL_PATH M=`pwd` $2

    if [ $? != 0 ]
    then
       exit -1
    fi
    
    make -C test $2
    
    if [ $? == 0 ]
    then
        if [ "$2" != "clean" -a "$2" != "debug" ]
        then
            cp *.ko $QEMU_PATH/update
            cp test/$TEST_TARGET $QEMU_PATH/update

            $QEMU_PATH/../run.sh
        elif [ "$2" == "debug" ]
        then
             cp *.ko $QEMU_PATH
            $QEMU_PATH/../rundebug.sh
        fi
    fi
elif [ "$1" = "bbb" ]; then
    KERNEL_PATH=`pwd`/../../../bb_soft/build/linux/
    docker run --rm=true -v "$(pwd):/home/user/linux_camp_modules" -v "$KERNEL_PATH:/home/user/linux_camp" build-bbb-eabi \
           "cd /home/user/linux_camp_modules && make -C /home/user/linux_camp  M=/home/user/linux_camp_modules C=1 $2"

    docker run --rm=true -v "$(pwd)/test:/home/user/linux_camp" -v "$(pwd):/home/user/linux_camp_modules"  build-bbb-linux-gnueabi \
           "make -C /home/user/linux_camp $2"
    if [ "$2" != "clean" -a "$2" != "debug" ]
    then
        cp *.ko ../../../bb_soft/build/busybox/_install
        cp test/$TEST_TARGET  ../../../bb_soft/build/busybox/_install
    fi
else
    QEMU_PATH=`pwd`/../../../qemu/build/
    KERNEL_PATH=${QEMU_PATH}linux-4.17.2/

    docker run --rm=true -v "$(pwd):/home/user/module" -v "$KERNEL_PATH:/home/user/linux_camp" indent-x86 

fi
