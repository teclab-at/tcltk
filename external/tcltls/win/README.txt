	Windows DLL Build instructions using nmake build system
	2020-10-15 Harald.Oehlmann@elmicron.de

Properties:
- 32 bit DLL
- VisualStudio 2015
Note: Vuisual C++ 6 does not build OpenSSL (long long syntax error)
- Cygwin32 (temporary helper, please help to replace by tclsh)
- OpenSSL statically linked to TCLTLS DLL.
Note: Dynamic linking also works but results in a DLL dependeny on OPENSSL DLL's

1) Build OpenSSL static libraries:

OpenSSL source distribtution unpacked in:
c:\test\tcltls\Openssl_1_1_1h

- Install Perl from http://strawberryperl.com/download/5.32.0.1/strawberry-perl-5.32.0.1-32bit.msi
  to C:\perl
  (ActivePerl failed due to missing 32 bit console module)
- Install NASM Assembler:

https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/win32/nasm-2.15.05-installer-x86.exe
  to C:\Program Files (x86)\NASM
  
-> Visual Studio x86 native prompt.

set Path=%PATH%;C:\Program Files (x86)\NASM;C:\Perl\perl\bin

perl Configure VC-WIN32 --prefix=c:\test\tcltls\openssl --openssldir=c:\test\tcltls\openssldir no-shared no-filenames threads

nmake
nmake test
namke install

2) Build TCLTLS

Unzip distribution in:
c:\test\tcltls\tcltls-1.7.22

-> start cygwin bash prompt

cd /cygdrive/c/test/tcltls/tcltls-1.7.22
./gen_dh_params > dh_params.h

od -A n -v -t xC < 'tls.tcl' > tls.tcl.h.new.1
sed 's@[^0-9A-Fa-f]@@g;s@..@0x&, @g' < tls.tcl.h.new.1 > tls.tcl.h
rm -f tls.tcl.h.new.1

-> Visual Studio x86 native prompt.

cd C:\test\tcltls\tcltls-1.7.22\win

nmake -f makefile.vc TCLDIR=c:\test\tcl8610 SSL_INSTALL_FOLDER=C:\test\tcltls\openssl

nmake -f makefile.vc install TCLDIR=c:\test\tcl8610 INSTALLDIR=c:\test\tcltls SSL_INSTALL_FOLDER=C:\test\tcltls\openssl

3) Test

Start tclsh or wish

lappend auto_path {C:\test\tcltls\tls1.7.22}
package require tls

A small "1.7.22" showing up is hopefully the end of this long way...