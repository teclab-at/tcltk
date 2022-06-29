dnl $1 = Name of variable
dnl $2 = Name of function to check for
dnl $3 = Name of protocol
dnl $4 = Name of CPP macro to define
dnl $5 = Name of CPP macro to check for instead of a function
AC_DEFUN([TCLTLS_SSL_OPENSSL_CHECK_PROTO_VER], [
	dnl Determine if particular SSL version is enabled
	if test "[$]$1" = "true" -o "[$]$1" = "force"; then
		proto_check='true'
		ifelse($5,, [
			AC_CHECK_FUNC($2,, [
				proto_check='false'
			])
		], [
			AC_LANG_PUSH(C)
			AC_MSG_CHECKING([for $3 protocol support])
			AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#if (SSLEAY_VERSION_NUMBER >= 0x0907000L)
# include <openssl/conf.h>
#endif
			], [
int x = $5;
			])], [
				AC_MSG_RESULT([yes])
			], [
				AC_MSG_RESULT([no])

				proto_check='false'
			])
			AC_LANG_POP([C])
		])

		if test "$proto_check" = 'false'; then
			if test "[$]$1" = "force"; then
				AC_MSG_ERROR([Unable to enable $3])
			fi

			$1='false'
		fi
	fi

	if test "[$]$1" = "false"; then
		AC_DEFINE($4, [1], [Define this to disable $3 in OpenSSL support])
	fi

])

AC_DEFUN([TCLTLS_SSL_OPENSSL], [
	openssldir=''
	opensslpkgconfigdir=''
	AC_ARG_WITH([ssl-dir],
		AS_HELP_STRING(
			[--with-ssl-dir=<dir>],
			[deprecated, use --with-openssl-dir -- currently has the same meaning]
		), [
			openssldir="$withval"
		]
	)
	AC_ARG_WITH([openssl-dir],
		AS_HELP_STRING(
			[--with-openssl-dir=<dir>],
			[path to root directory of OpenSSL or LibreSSL installation]
		), [
			openssldir="$withval"
		]
	)
	AC_ARG_WITH([openssl-pkgconfig],
		AS_HELP_STRING(
			[--with-openssl-pkgconfig=<dir>],
			[path to root directory of OpenSSL or LibreSSL pkgconfigdir]
		), [
			opensslpkgconfigdir="$withval"
		]
	)

	if test -n "$openssldir"; then
		if test -e "$openssldir/libssl.$SHOBJEXT"; then
			TCLTLS_SSL_LIBS="-L$openssldir -lssl -lcrypto"
			openssldir="`AS_DIRNAME(["$openssldir"])`"
		else
			TCLTLS_SSL_LIBS="-L$openssldir/lib -lssl -lcrypto"
		fi
		TCLTLS_SSL_CFLAGS="-I$openssldir/include"
		TCLTLS_SSL_CPPFLAGS="-I$openssldir/include"
	fi

	pkgConfigExtraArgs=''
	if test "$TCLEXT_BUILD" = "static" -o "$TCLEXT_TLS_STATIC_SSL" = 'yes'; then
		pkgConfigExtraArgs='--static'
	fi

	dnl Use pkg-config to find the libraries
	dnl Temporarily update PKG_CONFIG_PATH
	PKG_CONFIG_PATH_SAVE="${PKG_CONFIG_PATH}"
	if test -n "${opensslpkgconfigdir}"; then
		if ! test -f "${opensslpkgconfigdir}/openssl.pc"; then
			AC_MSG_ERROR([Unable to locate ${opensslpkgconfigdir}/openssl.pc])
		fi

		PKG_CONFIG_PATH="${opensslpkgconfigdir}${PATH_SEPARATOR}${PKG_CONFIG_PATH}"
		export PKG_CONFIG_PATH
	fi

	AC_ARG_VAR([TCLTLS_SSL_LIBS], [libraries to pass to the linker for OpenSSL or LibreSSL])
	AC_ARG_VAR([TCLTLS_SSL_CFLAGS], [C compiler flags for OpenSSL or LibreSSL])
	AC_ARG_VAR([TCLTLS_SSL_CPPFLAGS], [C preprocessor flags for OpenSSL or LibreSSL])
	if test -z "$TCLTLS_SSL_LIBS"; then
		TCLTLS_SSL_LIBS="`"${PKGCONFIG}" openssl --libs $pkgConfigExtraArgs`" || AC_MSG_ERROR([Unable to get OpenSSL Configuration])
	fi
	if test -z "$TCLTLS_SSL_CFLAGS"; then
		TCLTLS_SSL_CFLAGS="`"${PKGCONFIG}" openssl --cflags-only-other $pkgConfigExtraArgs`" || AC_MSG_ERROR([Unable to get OpenSSL Configuration])
	fi
	if test -z "$TCLTLS_SSL_CPPFLAGS"; then
		TCLTLS_SSL_CPPFLAGS="`"${PKGCONFIG}" openssl --cflags-only-I $pkgConfigExtraArgs`" || AC_MSG_ERROR([Unable to get OpenSSL Configuration])
	fi
	PKG_CONFIG_PATH="${PKG_CONFIG_PATH_SAVE}"

	if test "$TCLEXT_BUILD" = "static"; then
		dnl If we are doing a static build, save the linker flags for other programs to consume
		rm -f tcltls.${AREXT}.linkadd
		AS_ECHO(["$TCLTLS_SSL_LIBS"]) > tcltls.${AREXT}.linkadd
	fi

	dnl If we have been asked to statically link to the SSL library, specifically tell the linker to do so
	if test "$TCLEXT_TLS_STATIC_SSL" = 'yes'; then
		dnl Don't bother doing this if we aren't actually doing the runtime linking
		if test "$TCLEXT_BUILD" != "static"; then
			dnl Split the libraries into SSL and non-SSL libraries
			new_TCLTLS_SSL_LIBS_normal=''
			new_TCLTLS_SSL_LIBS_static=''
			for arg in $TCLTLS_SSL_LIBS; do
				case "${arg}" in
					-L*)
						new_TCLTLS_SSL_LIBS_normal="${new_TCLTLS_SSL_LIBS_normal} ${arg}"
						new_TCLTLS_SSL_LIBS_static="${new_TCLTLS_SSL_LIBS_static} ${arg}"
						;;
					-ldl|-lrt|-lc|-lpthread|-lm|-lcrypt|-lidn|-lresolv|-lgcc|-lgcc_s)
						new_TCLTLS_SSL_LIBS_normal="${new_TCLTLS_SSL_LIBS_normal} ${arg}"
						;;
					-l*)
						new_TCLTLS_SSL_LIBS_static="${new_TCLTLS_SSL_LIBS_static} ${arg}"
						;;
					*)
						new_TCLTLS_SSL_LIBS_normal="${new_TCLTLS_SSL_LIBS_normal} ${arg}"
						;;
				esac
			done
			SHOBJ_DO_STATIC_LINK_LIB([OpenSSL], [$new_TCLTLS_SSL_LIBS_static], [new_TCLTLS_SSL_LIBS_static])
			TCLTLS_SSL_LIBS="${new_TCLTLS_SSL_LIBS_normal} ${new_TCLTLS_SSL_LIBS_static}"
		fi
	fi

	dnl Save compile-altering variables we are changing
	SAVE_LIBS="${LIBS}"
	SAVE_CFLAGS="${CFLAGS}"
	SAVE_CPPFLAGS="${CPPFLAGS}"

	dnl Update compile-altering variables to include the OpenSSL libraries
	LIBS="${TCLTLS_SSL_LIBS} ${SAVE_LIBS} ${TCLTLS_SSL_LIBS}"
	CFLAGS="${TCLTLS_SSL_CFLAGS} ${SAVE_CFLAGS} ${TCLTLS_SSL_CFLAGS}"
	CPPFLAGS="${TCLTLS_SSL_CPPFLAGS} ${SAVE_CPPFLAGS} ${TCLTLS_SSL_CPPFLAGS}"

	dnl Verify that basic functionality is there
	AC_LANG_PUSH(C)
	AC_MSG_CHECKING([if a basic OpenSSL program works])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#if (SSLEAY_VERSION_NUMBER >= 0x0907000L)
# include <openssl/conf.h>
#endif
		], [
  SSL_library_init();
  SSL_load_error_strings();
		])], [
		AC_MSG_RESULT([yes])
	], [
		AC_MSG_RESULT([no])
		AC_MSG_ERROR([Unable to compile a basic program using OpenSSL])
	])
	AC_LANG_POP([C])

	AC_CHECK_FUNCS([TLS_method])
	TCLTLS_SSL_OPENSSL_CHECK_PROTO_VER([tcltls_ssl_ssl2], [SSLv2_method], [sslv2], [NO_SSL2])
	TCLTLS_SSL_OPENSSL_CHECK_PROTO_VER([tcltls_ssl_ssl3], [SSLv3_method], [sslv3], [NO_SSL3])
	TCLTLS_SSL_OPENSSL_CHECK_PROTO_VER([tcltls_ssl_tls1_0], [TLSv1_method], [tlsv1.0], [NO_TLS1])
	TCLTLS_SSL_OPENSSL_CHECK_PROTO_VER([tcltls_ssl_tls1_1], [TLSv1_1_method], [tlsv1.1], [NO_TLS1_1])
	TCLTLS_SSL_OPENSSL_CHECK_PROTO_VER([tcltls_ssl_tls1_2], [TLSv1_2_method], [tlsv1.2], [NO_TLS1_2])
	TCLTLS_SSL_OPENSSL_CHECK_PROTO_VER([tcltls_ssl_tls1_3], [], [tlsv1.3], [NO_TLS1_3], [SSL_OP_NO_TLSv1_3])

	AC_CACHE_VAL([tcltls_cv_func_tlsext_hostname], [
		AC_LANG_PUSH(C)
		AC_MSG_CHECKING([for SSL_set_tlsext_host_name])
		AC_LINK_IFELSE([AC_LANG_PROGRAM([
#include <openssl/ssl.h>
#if (SSLEAY_VERSION_NUMBER >= 0x0907000L)
# include <openssl/conf.h>
#endif
			], [
  (void)SSL_set_tlsext_host_name((void *) 0, (void *) 0);
			])], [
			AC_MSG_RESULT([yes])
			tcltls_cv_func_tlsext_hostname='yes'
		], [
			AC_MSG_RESULT([no])
			tcltls_cv_func_tlsext_hostname='no'
		])
		AC_LANG_POP([C])
	])

	if test "$tcltls_cv_func_tlsext_hostname" = 'no'; then
		AC_DEFINE([OPENSSL_NO_TLSEXT], [1], [Define this if your OpenSSL does not support the TLS Extension for SNI])
	fi

	dnl Restore compile-altering variables
	LIBS="${SAVE_LIBS}"
	CFLAGS="${SAVE_CFLAGS}"
	CPPFLAGS="${SAVE_CPPFLAGS}"
])
