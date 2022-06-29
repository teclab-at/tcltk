#!/bin/bash -e

## module
MODULE=tcltls

## load defaults
. ./common.sh

## check debug switch
if [ -n "$SW_DEBUG" ]; then
    CONFIGURE_SWITCH+="--enable-symbols "
fi

## check 64bit platform
if [ "$BARCH" == "x86_64" ]; then
    CONFIGURE_SWITCH+="--enable-64bit "
fi

## print
echo "** Compiling $MODULE $CONFIGURE_SWITCH"

## delete whole compile sources and start from new
if [ -n "$SW_CLEANUP" ]; then
    echo -n "Deleting old sources ... "
    rm -rf $COMPILEDIR/$MODULE
    echo done
fi
## copy source to temp compile path
if [ ! -d $COMPILEDIR/$MODULE ] || [ -n "$SW_FORCECOPY" ]; then
    echo -n "Copying new sources ... "
    rsync -a --exclude "*/.git" --exclude "*.fossil" --exclude "_FOSSIL_" $EXTSRCDIR/$MODULE $COMPILEDIR/
    rm -rf $COMPILEDIR/$MODULE/.fossil-settings
    echo done
fi

## configure and make
cd $COMPILEDIR/$MODULE
./autogen.sh
./configure --prefix=$INSTALLDIR --mandir=$iSHAREDIR --enable-static-ssl
make
make install

## fini
echo "** Finished compile-$MODULE."
