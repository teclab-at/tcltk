#!/bin/bash -e

## module
MODULE=tk

## load defaults
. ./common.sh

## check debug switch
if [ -n "$SW_DEBUG" ]; then
    CONFIGURE_SWITCH+="--enable-symbols "
fi
## check symbol switch
if [ -n "$SW_SYMBOLS" ]; then
    CFLAGS="-g"
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
if [ ! -d $COMPILEDIR/$MODULE ] || [ -n "$SW_FORCECOPY" ] ; then
    echo -n "Copying new sources ... "
    rsync -a --exclude "*/.git" $EXTSRCDIR/$MODULE $COMPILEDIR/
    rsync -a $PATCHDIR/$MODULE/wish.ico $COMPILEDIR/$MODULE/win/rc/
    rsync -a $PATCHDIR/$MODULE/wish.ico $COMPILEDIR/$MODULE/win/rc/tk.ico
    echo done
    ## apply patches
    ## https://sourceforge.net/tracker/?func=detail&aid=2996762&group_id=12997&atid=312997
    patch --forward $COMPILEDIR/$MODULE/generic/tkListbox.c $PATCHDIR/$MODULE/tk-lbjustify-tkListbox-c.patch
    if [ -z "$WINDIR" ]; then
        patch --forward $COMPILEDIR/$MODULE/unix/tkUnixDefault.h $PATCHDIR/$MODULE/tk-lbjustify-tkUnixDefault-h.patch
    else
        patch --forward $COMPILEDIR/$MODULE/win/tkWinDefault.h $PATCHDIR/$MODULE/tk-lbjustify-tkWinDefault-h.patch
    fi
fi

## change to compile dir
if [ -z "$WINDIR" ]; then
    cd $COMPILEDIR/$MODULE/unix
else
    cd $COMPILEDIR/$MODULE/win
fi

## configure and make
(CFLAGS="$CFLAGS" ./configure --prefix=$INSTALLDIR --enable-threads --mandir=$iSHAREDIR $CONFIGURE_SWITCH)
## --with-tcl=$INSTALLDIR/lib --disable-xft
make
make install

echo -n "Copying extras ... "
## postprocess
cd $INSTALLDIR/bin
if [ -z "$WINDIR" ]; then
    mv wish8.5 wish85
    rsync -a wish85 wish
else
    if [ -n "$SW_DEBUG" ]; then
        mv wish85g.exe wish85.exe
        rsync -a wish85.exe wish.exe
    else
        rsync -a wish85.exe wish.exe
    fi
fi
  
## copy licence file
rsync -a $EXTSRCDIR/$MODULE/license.terms $iLICENCEDIR/$MODULE.licence
echo done

## fini
echo "** Finished compile-$MODULE."
