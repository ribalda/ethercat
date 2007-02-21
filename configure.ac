#------------------------------------------------------------------------------
# $Id$
#------------------------------------------------------------------------------

AC_PREREQ(2.59)
AC_INIT([ethercat],[1.2.0],[fp@igh-essen.com])
AC_CONFIG_AUX_DIR([autoconf])
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2])
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
           AC_MSG_ERROR([Failed to find Linux sources. Use --with-linux-dir!])
        fi
        sourcedir=`cd ${modulesdir}/build && pwd -P`
    ]
)

AC_MSG_CHECKING([for Linux kernel sources])

if test \! -r ${sourcedir}/.config; then
    echo
    AC_MSG_ERROR([No configured Linux kernel sources in $sourcedir])
fi

# Try to get kernel release string
if test -r ${sourcedir}/include/config/kernel.release; then
    kernelrelease=`cat $sourcedir/include/config/kernel.release`
elif test -r ${sourcedir}/.kernelrelease; then
    kernelrelease=`cat $sourcedir/.kernelrelease`
elif test -r ${sourcedir}/include/linux/utsrelease.h; then
    hdr=${sourcedir}/include/linux/utsrelease.h
    kernelrelease=`grep UTS_RELEASE $hdr | cut -d " " -f 3- | tr -d \"`
elif test -r ${sourcedir}/include/linux/version.h; then
    hdr=${sourcedir}/include/linux/version.h
    kernelrelease=`grep UTS_RELEASE $hdr | cut -d " " -f 3- | tr -d \"`
fi

if test -z "$kernelrelease"; then
    echo
    AC_MSG_ERROR([Failed to extract Linux kernel version!])
fi

# Extract three numbers from kernel release string
linuxversion=`echo $kernelrelease | grep -oE "^[[0-9]]+\.[[0-9]]+\.[[0-9]]+"`

AC_SUBST(LINUX_SOURCE_DIR,[$sourcedir])
AC_SUBST(LINUX_KERNEL_RELEASE,[$kernelrelease])
AC_SUBST(LINUX_KERNEL_VERSION,[$linuxversion])
AC_SUBST(LINUX_MOD_PATH,[/lib/modules/$kernelrelease/ethercat])
AC_MSG_RESULT([$LINUX_SOURCE_DIR (Kernel $LINUX_KERNEL_RELEASE)])

#------------------------------------------------------------------------------
# 8139too driver
#------------------------------------------------------------------------------

AC_ARG_ENABLE([8139too],
    AS_HELP_STRING([--enable-8139too],
                   [Enable 8139too driver]),
    [
        case "${enableval}" in
            yes) enable8139too=1
                ;;
            no) enable8139too=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-8139too])
                ;;
        esac
    ],
    [enable8139too=1]
)

AM_CONDITIONAL(ENABLE_8139TOO, test "x$enable8139too" = "x1")

AC_ARG_WITH([8139too-kernel],
    AC_HELP_STRING(
        [--with-8139too-kernel=<X.Y.Z>],
        [8139too kernel (only if differing)]
    ),
    [
        kernel8139too=[$withval]
    ],
    [
        kernel8139too=$linuxversion
    ]
)

if test "x${enable8139too}" = "x1"; then
    AC_MSG_CHECKING([for kernel for 8139too driver])

    kernels=`ls -1 devices/ | grep -oE "^8139too-.*-" | cut -d "-" -f 2 | uniq`
    found=0
    for k in $kernels; do
        if test "$kernel8139too" = "$k"; then
            found=1
        fi
    done
    if test $found -ne 1; then
        AC_MSG_ERROR([kernel $kernel8139too not available for 8139too driver!])
    fi

    AC_MSG_RESULT([$kernel8139too])
fi

#------------------------------------------------------------------------------
# e100 driver
#------------------------------------------------------------------------------

AC_ARG_ENABLE([e100],
    AS_HELP_STRING([--enable-e100],
                   [Enable e100 driver]),
    [
        case "${enableval}" in
            yes) enablee100=1
                ;;
            no) enablee100=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-e100])
                ;;
        esac
    ],
    [enablee100=0] # disabled by default
)

AM_CONDITIONAL(ENABLE_E100, test "x$enablee100" = "x1")

AC_ARG_WITH([e100-kernel],
    AC_HELP_STRING(
        [--with-e100-kernel=<X.Y.Z>],
        [e100 kernel (only if differing)]
    ),
    [
        kernele100=[$withval]
    ],
    [
        kernele100=$linuxversion
    ]
)

if test "x${enablee100}" = "x1"; then
    AC_MSG_CHECKING([for kernel for e100 driver])

    kernels=`ls -1 devices/ | grep -oE "^e100-.*-" | cut -d "-" -f 2 | uniq`
    found=0
    for k in $kernels; do
        if test "$kernele100" = "$k"; then
            found=1
        fi
    done
    if test $found -ne 1; then
        AC_MSG_ERROR([kernel $kernele100 not available for e100 driver!])
    fi

    AC_MSG_RESULT([$kernele100])
fi

#------------------------------------------------------------------------------
# forcedeth driver
#------------------------------------------------------------------------------

AC_ARG_ENABLE([forcedeth],
    AS_HELP_STRING([--enable-forcedeth],
                   [Enable forcedeth driver]),
    [
        case "${enableval}" in
            yes) enableforcedeth=1
                ;;
            no) enableforcedeth=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-forcedeth])
                ;;
        esac
    ],
    [enableforcedeth=0] # disabled by default!
)

AM_CONDITIONAL(ENABLE_FORCEDETH, test "x$enableforcedeth" = "x1")

AC_ARG_WITH([forcedeth-kernel],
    AC_HELP_STRING(
        [--with-forcedeth-kernel=<X.Y.Z>],
        [forcedeth kernel (only if differing)]
    ),
    [
        kernelforcedeth=[$withval]
    ],
    [
        kernelforcedeth=$linuxversion
    ]
)

if test "x${enableforcedeth}" = "x1"; then
    AC_MSG_CHECKING([for kernel for forcedeth driver])

    kernels=`ls -1 devices/ | grep -oE "^forcedeth-.*-" | cut -d "-" -f 2 | uniq`
    found=0
    for k in $kernels; do
        if test "$kernelforcedeth" = "$k"; then
            found=1
        fi
    done
    if test $found -ne 1; then
        AC_MSG_ERROR([kernel $kernelforcedeth not available for forcedeth driver!])
    fi

    AC_MSG_RESULT([$kernelforcedeth])
fi

#------------------------------------------------------------------------------
# RTAI path (optional)
#------------------------------------------------------------------------------

AC_ARG_WITH([rtai-dir],
    AC_HELP_STRING(
        [--with-rtai-dir=<DIR>],
        [RTAI path (only for RTAI examples)]
    ),
    [
        rtaidir=[$withval]
    ],
    [
        rtaidir=""
    ]
)

AC_MSG_CHECKING([for RTAI path])

if test -z "${rtaidir}"; then
    AC_MSG_RESULT([not specified.])
else
    if test \! -r ${rtaidir}/include/rtai.h; then
        AC_MSG_ERROR([no RTAI installation found in ${rtaidir}!])
    fi
    AC_MSG_RESULT([$rtaidir])
    AC_SUBST(RTAI_DIR,[$rtaidir])
fi

#------------------------------------------------------------------------------
# MSR path (optional)
#------------------------------------------------------------------------------

AC_ARG_WITH([msr-dir],
    AC_HELP_STRING(
        [--with-msr-dir=<DIR>],
        [MSR path (only for MSR example)]
    ),
    [
        msrdir=[$withval]
    ],
    [
        msrdir=""
    ]
)

AC_MSG_CHECKING([for MSR path])

if test -z "${msrdir}"; then
    AC_MSG_RESULT([not specified.])
else
    if test \! -r ${msrdir}/include/msr.h; then
        AC_MSG_ERROR([no MSR installation found in ${msrdir}!])
    fi
    AC_MSG_RESULT([$msrdir])
    AC_SUBST(MSR_DIR,[$msrdir])
fi

#------------------------------------------------------------------------------
# Debug interface
#------------------------------------------------------------------------------

AC_ARG_ENABLE([dbg-if],
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

AM_CONDITIONAL(EC_DBG_IF, test "x$dbg" = "x1")
AC_SUBST([EC_DBG_IF],${dbg})

#------------------------------------------------------------------------------

# Create config.kbuild

echo configure: creating config.kbuild...

cat > config.kbuild <<EOF
# config.kbuild - created by configure
EC_DBG_IF := ${dbg}
EC_ENABLE_8139TOO := ${enablee100}
EC_8139TOO_KERNEL := ${kernel8139too}
EC_ENABLE_E100 := ${enablee100}
EC_E100_KERNEL := ${kernele100}
EC_ENABLE_FORCEDETH := ${enableforcedeth}
EC_FORCEDETH_KERNEL := ${kernelforcedeth}
EC_RTAI_DIR := "${rtaidir}"
EC_MSR_DIR := "${msrdir}"
EOF

#------------------------------------------------------------------------------

AC_CONFIG_FILES([
        Makefile
        master/Makefile
        devices/Makefile
        script/Makefile
        script/init.d/Makefile
        script/sysconfig/Makefile
        include/Makefile
        examples/Makefile
        examples/mini/Makefile
        examples/rtai/Makefile
        examples/msr/Makefile
])
AC_OUTPUT

#------------------------------------------------------------------------------
