#------------------------------------------------------------------------------
# $Id$
#------------------------------------------------------------------------------

AC_INIT([ethercat],[1.1],[fp@igh-essen.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign dist-bzip2])
AC_PREFIX_DEFAULT([/opt/etherlab])

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

AC_CONFIG_FILES([
        Makefile 
        master/Makefile 
        devices/Makefile 
        script/Makefile
        examples/mini/Makefile
	examples/rtai/Makefile
	examples/msr/Makefile
])
AC_OUTPUT

#------------------------------------------------------------------------------
