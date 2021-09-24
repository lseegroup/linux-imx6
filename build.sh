#!/bin/bash
export CPUS=`grep -c processor /proc/cpuinfo`
DEVTYPE=$1
make distclean
make okmx6ul_defconfig 
make zImage -j${CPUS}
make dtbs -j${CPUS}
make modules -j${CPUS}
