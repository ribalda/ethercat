#------------------------------------------------------------------------------
# $Id$
#------------------------------------------------------------------------------

AC_PREREQ(2.59)
AC_INIT([ethercat],[1.1],[fp@igh-essen.com])
AC_CONFIG_AUX_DIR([autoconf])
AM_INIT_AUTOMAKE([-Wall -Werror foreign dist-bzip2])
AC_PREFIX_DEFAULT([/opt/etherlab])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([config.h.in])

#------------------------------------------------------------------------------
# Linux sources
#------------------------------------------------------------------------------

AC_ARG_WITH([linux],
    AC_HELP_STRING(
        [--with-linux=<version>],
        [Linux kernel version @<:@running kernel@:>@]
    ),
    [version=[$withval]],
    [version=[`uname -r`]]
)

AC_MSG_CHECKING([for Linux kernel])
modulesdir=/lib/modules/${version}
if test \! -d ${modulesdir} || test \! -d ${modulesdir}/build; then
   echo
   AC_MSG_ERROR([Invalid modules directory ${modulesdir}])
fi
sourcedir=`cd ${modulesdir}/build && pwd -P`
if test \! -r ${sourcedir}/Makefile; then
   echo
   AC_MSG_ERROR([No Linux kernel sources in $sourcedir])
fi

AC_SUBST(LINUX_KERNEL_VERSION,[$version])
AC_SUBST(LINUX_SOURCE_DIR,[$sourcedir])
AC_SUBST(LINUX_MODULES_DIR,[$modulesdir])

AC_MSG_RESULT($LINUX_KERNEL_VERSION)

#------------------------------------------------------------------------------
# Depmod
#------------------------------------------------------------------------------

AC_PATH_PROG([DEPMOD], [depmod],, [$PATH:/sbin:/usr/sbin:/usr/local/sbin])

if test -z "$DEPMOD"; then
   AC_MSG_WARN([depmod was not found!]);
fi

#------------------------------------------------------------------------------
# Debug interface
#------------------------------------------------------------------------------

AC_ARG_ENABLE([debug-if],
		AS_HELP_STRING([--enable-dbg-if],
						[Create a debug interface for each master @<:@NO@:>@]),
		[case "${enableval}" in
			  yes) dbg=1
				   AC_DEFINE([EC_DBG_IF], [1], [Debug interfaces enabled])
				   ;;
			  no)  dbg=0
				   ;;
			  *)   AC_MSG_ERROR([Invalid value for --enable-dbg-if])
				   ;;
		esac],
		[dbg=0]
)
AM_CONDITIONAL(EC_DBG_IF, test "x$dbg" = x1)
AC_SUBST([EC_DBG_IF],${dbg})

#------------------------------------------------------------------------------

AC_CONFIG_FILES([
        Makefile
        master/Makefile
        devices/Makefile
        script/Makefile
        include/Makefile
        examples/mini/Makefile
        examples/rtai/Makefile
        examples/msr/Makefile
])
AC_OUTPUT

#------------------------------------------------------------------------------
