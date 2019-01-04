#!/bin/bash

if [ $# -ne 3 ]; then
    echo "Need 3 arguments: 1) kernel source dir, 2) previous version, 3) version to add"
    exit 1
fi

KERNELDIR=$1
PREVER=$2
KERNELVER=$3

set -x

for f in $KERNELDIR/drivers/net/ethernet/intel/e1000e/*.[ch]; do
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
    sed -i s/$PREVER-ethercat.h/$KERNELVER-ethercat.h/ $e
    hg add $o $e
done
