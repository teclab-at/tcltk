#!/bin/bash -e
## exit on non-zero status
set -e

## info
VERSION=1.4
MANUFACT="(c) 2018 ATOS"
AUTHORS="Thomas Perschak"

## defaults
SCRIPTDIR=$(dirname $(readlink -f $0))

## helper functions
##
function printHelp {
    echo "TclTk environment compiler script."
    echo "$MANUFACT, $AUTHORS"
    echo "Script version: $VERSION"
    echo ""
    echo "Syntax:"
    echo " compile-<module>.sh [<options>]"
    echo "Options:"
    echo " -debug: Compile using debug symbols and no optimizations."
    echo " -symbols: Include debug symbols."
    echo " -clean: Delete old module compile directory before copying."
    echo " -forcecopy: Copy module sources to compile directory even if the directory already exists."
    echo " -help: Print this help text."
}
##
## helper functions

## module
TCL_EXECREL=85
WISH=wish$TCL_EXECREL
TCLSH=tclsh$TCL_EXECREL

## check command line parameters
INDEX=1
while [ $INDEX -le $# ]; do
    case ${!INDEX} in
        "-debug")
            SW_DEBUG=1
            ;;
        "-symbols")
            SW_SYMBOLS=1
            ;;
        "-nochlog")
            SW_NOCHLOG=1
            ;;
        "-clean")
            SW_CLEANUP=1
            ;;
        "-forcecopy")
            SW_FORCECOPY=1
            ;;
        "-dummy")
            let INDEX+=1
            SW_DUMMY=${!INDEX}
            ;;
        "-help")
            printHelp
            exit 1
            ;;
        *)
            echo "Unknown parameter '${!INDEX}', type -help for more information." >&2; exit 1
            ;;
    esac
    let INDEX+=1
done

## specific debug settings
if [ -n "$SW_DEBUG" ]; then
    INSTALLDIR=$SCRIPTDIR/debug
    COMPILEDIR=$SCRIPTDIR/dcompile
else
    INSTALLDIR=$SCRIPTDIR/release
    COMPILEDIR=$SCRIPTDIR/rcompile
fi

## commons
INTSRCDIR=$SCRIPTDIR/internal
EXTSRCDIR=$SCRIPTDIR/external
PATCHDIR=$SCRIPTDIR/patch
TCLLIBDIR=$COMPILEDIR/tcllib
TCLDIR=$COMPILEDIR/tcl
TKDIR=$COMPILEDIR/tk
SHAREDIR=$SCRIPTDIR/share
LICENCEDIR=$SHAREDIR/licence
CHANGELOGDIR=$SHAREDIR/changelog
iSHAREDIR=$INSTALLDIR/share
iLICENCEDIR=$iSHAREDIR/licence
iCHANGELOGDIR=$iSHAREDIR/changelog
iSHARELIB64DIR=$iSHAREDIR/lib64

## plattform specific
if [ -z "$WINDIR" ]; then
    ## linux platform
    LIBEXT=.so
    ## extend the search path
    PATH=$INSTALLDIR/bin:$TCLLIBDIR/apps:$PATH

    ## get base architecture
    BARCH=$(uname -m)
else
    ## windows platform
    LIBEXT=.dll
    ## find location of postgres
    for PGDIR in "C:/Program Files/PostgreSQL/11" "C:/Program Files/PostgreSQL/9.5" "D:/Programs/PostgreSQL" "C:/Program Files/PostgreSQL/9.4" "C:/Program Files/PostgreSQL/9.3" "C:/Program Files/PostgreSQL/9.2"; do
        if [ -e "$PGDIR" ]; then break; fi
    done
    OSSLDIR="D:/Programs/OpenSSL-Win32"
    ## extend the search path
    PATH=$INSTALLDIR/bin:$TCLLIBDIR/apps:$PATH

    ## check MinGW gcc build if its 64bit
    gcc --version | grep -q MinGW-W64 | true
    if [ ${PIPESTATUS[1]} -eq 0 ]; then
        BARCH="x86_64"
    else
        BARCH=$(uname -m)
    fi
fi

## check if we got the correct architecture recognized
case $BARCH in
    "i686")
        ;;
    "x86_64")
        ;;
    *)
        echo "Unknown base architecture '$BARCH'." >&2; exit 1
        ;;
esac

## check what command shall be used for dos to unix conversion
if [ -n "$(type dos2unix)" ]; then DOS2NIX=dos2unix; fi > /dev/null 2>&1
if [ -n "$(type fromdos)" ]; then DOS2NIX=fromdos; fi > /dev/null 2>&1

## create temp and output dirs
function createDirs {
    mkdir -p $INSTALLDIR
    mkdir -p $COMPILEDIR
    mkdir -p $iCHANGELOGDIR
    if [ -n "$WINDIR" ]; then
        mkdir -p $iSHARELIB64DIR
    fi
}

function dos2nix {
    echo -n "Converting from DOS to NIX ... "
    ## removing carriage return
    find . -type f -name Makefile -print0 | xargs -0 $DOS2NIX -f -o 2> /dev/null
    find . -type f -iname README -print0 | xargs -0 $DOS2NIX -f -o 2> /dev/null
    find . -type f -name configure -print0 | xargs -0 $DOS2NIX -f -o 2> /dev/null
    find . -type f -name config.sub -print0 | xargs -0 $DOS2NIX -f -o 2> /dev/null
    find . -type f -name config.guess -print0 | xargs -0 $DOS2NIX -f -o 2> /dev/null
    find . -type f -name dtplite -print0 | xargs -0 $DOS2NIX -f -o 2> /dev/null
    find . -type f -iregex ".*\([.]in\|[.]sh|[.]man|[.]htm|[.]css|[.]inc|[.]tcl|[.]itk|[.]c|[.]h|[.]txt|[.]decls\)" -print0 | xargs -0 $DOS2NIX -f -o 2> /dev/null
    ## change execution rights
    echo done
    echo -n "Changing permission bits ... "
    find . -type f -name configure -print0 | xargs -0 chmod ug+x
    echo done    
}

