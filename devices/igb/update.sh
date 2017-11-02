#!/bin/bash

set -x

KERNELDIR=/data/kernel/linux-3.16.6
PREVER=3.14
KERNELVER=3.16

for f in $KERNELDIR/drivers/net/ethernet/intel/igb/*.[ch]; do
    echo $f
    b=$(basename $f)
    o=${b/\./-$KERNELVER-orig.}
    e=${b/\./-$KERNELVER-ethercat.}
    cp -v $f $o
    chmod 644 $o
    cp -v $o $e
    op=${b/\./-$PREVER-orig.}
    ep=${b/\./-$PREVER-ethercat.}
    diff -up $op $ep | patch -p1 $e
    sed -i s/$PREVER-ethercat.h/$KERNELVER-ethercat.h/ $e
    hg add $o $e
done
