#!/bin/bash -e

## module
MODULE=tklib

## load defaults
. ./common.sh

## print
echo "** Compiling $MODULE"

## delete whole compile sources and start from new
if [ -n "$SW_CLEANUP" ]; then
    echo -n "Deleting old sources ... "
    rm -rf $COMPILEDIR/$MODULE
    echo done
fi
## copy source to temp compile path
if [ ! -d $COMPILEDIR/$MODULE ] || [ -n "$SW_FORCECOPY" ]; then
    echo -n "Copying new sources ... "
    rsync -a --exclude "*/.git" $EXTSRCDIR/$MODULE $COMPILEDIR/
    echo done
fi

## change to compile dir
cd $COMPILEDIR/$MODULE

## configure and make
./configure --prefix=$INSTALLDIR --exec-prefix=$INSTALLDIR --mandir=$iSHAREDIR
make install
chmod ug+x $INSTALLDIR/bin/*

echo -n "Copying extras ... "
## copy help files
mkdir -p $iSHAREDIR/doc/$MODULE
rsync -a $COMPILEDIR/$MODULE/idoc/www/* $iSHAREDIR/doc/$MODULE/

## copy missing files
## seems to be fixed
## rsync -a $COMPILEDIR/$MODULE/modules/tablelist/scripts/pencil.cur $INSTALLDIR/lib/${MODULE}0.6/tablelist/scripts/

## copy licence file
rsync -a $EXTSRCDIR/$MODULE/license.terms $iLICENCEDIR/$MODULE.licence
echo done

## fini
echo "** Finished compile-$MODULE."
