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
# Global
#------------------------------------------------------------------------------

branch=trunk

AC_DEFINE_UNQUOTED(BRANCH, ["$branch"], [Subversion branch])
AC_SUBST(BRANCH, [$branch])

#------------------------------------------------------------------------------
# Linux sources
#------------------------------------------------------------------------------

AC_ARG_WITH([linux-dir],
    AC_HELP_STRING(
        [--with-linux-dir=<DIR>],
        [Linux kernel sources @<:@running kernel@:>@]
    ),
    [
        sourcedir=[$withval]
    ],
    [
        version=[`uname -r`]
        modulesdir=/lib/modules/${version}
        if test \! -d ${modulesdir} || test \! -d ${modulesdir}/build; then
           echo
           AC_MSG_ERROR([Invalid modules directory ${modulesdir}])
        fi
        sourcedir=`cd ${modulesdir}/build && pwd -P`
    ]
)

AC_MSG_CHECKING([for Linux kernel sources])

if test \! -r ${sourcedir}/.config; then
   echo
   AC_MSG_ERROR([No configured Linux kernel sources in $sourcedir])
fi

AC_SUBST(LINUX_SOURCE_DIR,[$sourcedir])
AC_MSG_RESULT($LINUX_SOURCE_DIR)

#------------------------------------------------------------------------------
# Debug interface
#------------------------------------------------------------------------------

AC_ARG_ENABLE([debug-if],
    AS_HELP_STRING([--enable-dbg-if],
                   [Create a debug interface for each master @<:@NO@:>@]),
    [
        case "${enableval}" in
            yes) dbg=1
                AC_DEFINE([EC_DBG_IF], [1], [Debug interfaces enabled])
                ;;
            no) dbg=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-dbg-if])
                ;;
        esac
    ],
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
        examples/Makefile
        examples/mini/Makefile
        examples/rtai/Makefile
        examples/msr/Makefile
])
AC_OUTPUT

#------------------------------------------------------------------------------
