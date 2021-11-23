#!/bin/bash -e
## exit on non-zero status
set -e

## info
VERSION=1.0
MANUFACT="(c) 2018 ATOS"
AUTHORS="Thomas Perschak"

## defaults
SCRIPTDIR=$(dirname $(readlink -f $0))

## init
INSTALLDIR=$SCRIPTDIR/release
SHAREDIR=$SCRIPTDIR/share
PATCHDIR=$SCRIPTDIR/patch
CHANGELOGDIR=$SHAREDIR/changelog
EXTSRCDIR=$SCRIPTDIR/external
iSHAREDIR=$INSTALLDIR/share
iCHANGELOGDIR=$iSHAREDIR/changelog

## helper functions
##
function printHelp {
    echo "Script creating Debian compliant change-log file from git commit history."
    echo "$MANUFACT, $AUTHORS"
    echo "Script version: $VERSION"
    echo ""
    echo "Syntax:"
    echo " build-history.sh [<options>]"
    echo "Options (optional):"
    echo " -help: Print this help text."
}
##
## helper functions

## read command line parameters
INDEX=1
while [ $INDEX -le $# ]; do
    case ${!INDEX} in
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

## check if gse4-service directory exists
if [ ! -d $SCRIPTDIR/../gse4-service ]; then
    echo "Missing ../gse4-service" >&2
    echo "Clone /cmdata/git/products/gse4-service.git into ../gse4-service to get the build environment." >&2
    exit 1
fi

## create a new debian directory from template
echo -n "Collecting build information ... "

## from control file
DEBNAME=$(awk -F ' ' '{if($1=="Package:") print $2}' $SCRIPTDIR/debian.template/control)

## done collecting build infos
echo done

## some repositories do have specific version patterns
function getvpattern()
{
    case $1 in
        "tclconfig") echo "tea_";;
        "tcl"|"tk") echo "core_";;
        *) echo "${1}_";;
    esac
}

## update the changelog file
$SCRIPTDIR/../gse4-service/scripts/git-changelog.sh -indir $SCRIPTDIR
## make a release copy
cp $SCRIPTDIR/debian.template/changelog $iCHANGELOGDIR/${DEBNAME}.changelog

## update the changelog files from external
for folder in $(cd $EXTSRCDIR; ls -1d *); do
    folder=${folder%/}
    if [ ! -d $EXTSRCDIR/$folder/.git ]; then continue; fi
    $SCRIPTDIR/../gse4-service/scripts/git-changelog.sh -indir $EXTSRCDIR/$folder -package $folder -vpattern $(getvpattern $folder) -outfile $CHANGELOGDIR/${folder}.changelog -sinced 1.year
    if [ ! -e $CHANGELOGDIR/${folder}.changelog ]; then
        $SCRIPTDIR/../gse4-service/scripts/git-changelog.sh -indir $EXTSRCDIR/$folder -package $folder -vpattern $(getvpattern $folder) -outfile $CHANGELOGDIR/${folder}.changelog -lastn 300
    fi
    ## make a release copy
    cp -f $CHANGELOGDIR/${folder}.changelog $iCHANGELOGDIR/ | true
done


