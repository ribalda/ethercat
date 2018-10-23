#!/bin/bash

set -x

KERNELDIR=/data/kernel/linux-4.4.162
PREVER=3.16
KERNELVER=4.4

for f in $KERNELDIR/drivers/net/ethernet/{realtek/8139too,realtek/r8169,intel/e100}.c; do
    echo $f
    b=$(basename $f)
    o=${b/\./-$KERNELVER-orig.}
    e=${b/\./-$KERNELVER-ethercat.}
    cp -v $f $o
    chmod 644 $o
    cp -v $o $e
    op=${b/\./-$PREVER-orig.}
    ep=${b/\./-$PREVER-ethercat.}
    diff -u $op $ep | patch -p1 $e
    hg add $o $e
done
