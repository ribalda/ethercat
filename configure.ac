#------------------------------------------------------------------------------
# $Id$
#------------------------------------------------------------------------------

AC_INIT([ethercat],[1.1],[fp@igh-essen.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign dist-bzip2])
AC_PREFIX_DEFAULT([/opt/etherlab])
AC_CONFIG_FILES([Makefile master/Makefile devices/Makefile \
	         examples/mini/Makefile \
		 examples/rtai/Makefile \
		 examples/msr/Makefile])

#------------------------------------------------------------------------------
# Linux sources
#------------------------------------------------------------------------------

AC_ARG_WITH([linux-dir],
    AC_HELP_STRING(
        [--with-linux-dir=<linux-dir>],
        [Linux source directory @<:@/lib/modules/<kernelversion>/build@:>@]),
    [ ],
    [
	modulesdir=/lib/modules/`uname -r`
        withval=${modulesdir}/build
    ])

AC_MSG_CHECKING([for Linux source tree])

if test \! -r ${withval}/Makefile; then
   echo
   AC_MSG_ERROR([No Linux kernel tree in $withval])
fi

AC_SUBST(LINUX_DIR,[$withval])
AC_SUBST(LINUX_MODULES_DIR,[$modulesdir])

AC_MSG_RESULT($LINUX_DIR)

#------------------------------------------------------------------------------

AC_OUTPUT

#------------------------------------------------------------------------------
