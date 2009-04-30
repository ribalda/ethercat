#------------------------------------------------------------------------------
#
#  $Id$
#
#  Copyright (C) 2006-2009  Florian Pose, Ingenieurgemeinschaft IgH
#
#  This file is part of the IgH EtherCAT Master.
#
#  The IgH EtherCAT Master is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License version 2, as
#  published by the Free Software Foundation.
#
#  The IgH EtherCAT Master is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
#  Public License for more details.
#
#  You should have received a copy of the GNU General Public License along
#  with the IgH EtherCAT Master; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#  ---
#  
#  The license mentioned above concerns the source code only. Using the
#  EtherCAT technology and brand is only permitted in compliance with the
#  industrial property and similar rights of Beckhoff Automation GmbH.
#
#------------------------------------------------------------------------------

AC_PREREQ(2.59)
AC_INIT([ethercat],[devel],[fp@igh-essen.com])
AC_CONFIG_AUX_DIR([autoconf])
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2])
AC_PREFIX_DEFAULT([/opt/etherlab])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])

#------------------------------------------------------------------------------
# Global
#------------------------------------------------------------------------------

AC_PROG_CXX
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

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
AC_SUBST(ENABLE_8139TOO,[$enable8139too])

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

    kernels=`ls -1 ${srcdir}/devices/ | grep -oE "^8139too-.*-" | cut -d "-" -f 2 | uniq`
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

AC_SUBST(KERNEL_8139TOO,[$kernel8139too])

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
AC_SUBST(ENABLE_E100,[$enablee100])

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

    kernels=`ls -1 ${srcdir}/devices/ | grep -oE "^e100-.*-" | cut -d "-" -f 2 | uniq`
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

AC_SUBST(KERNEL_E100,[$kernele100])

#------------------------------------------------------------------------------
# e1000 driver
#------------------------------------------------------------------------------

AC_ARG_ENABLE([e1000],
    AS_HELP_STRING([--enable-e1000],
                   [Enable e1000 driver]),
    [
        case "${enableval}" in
            yes) enablee1000=1
                ;;
            no) enablee1000=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-e1000])
                ;;
        esac
    ],
    [enablee1000=0] # disabled by default
)

AM_CONDITIONAL(ENABLE_E1000, test "x$enablee1000" = "x1")
AC_SUBST(ENABLE_E1000,[$enablee1000])

AC_ARG_WITH([e1000-kernel],
    AC_HELP_STRING(
        [--with-e1000-kernel=<X.Y.Z>],
        [e1000 kernel (only if differing)]
    ),
    [
        kernele1000=[$withval]
    ],
    [
        kernele1000=$linuxversion
    ]
)

if test "x${enablee1000}" = "x1"; then
    AC_MSG_CHECKING([for kernel for e1000 driver])

    kernels=`ls -1 ${srcdir}/devices/e1000/ | grep -oE "^e1000_main-.*" | cut -d "-" -f 2 | uniq`
    found=0
    for k in $kernels; do
        if test "$kernele1000" = "$k"; then
            found=1
        fi
    done
    if test $found -ne 1; then
        AC_MSG_ERROR([kernel $kernele1000 not available for e1000 driver!])
    fi

    AC_MSG_RESULT([$kernele1000])
fi

AC_SUBST(KERNEL_E1000,[$kernele1000])

#------------------------------------------------------------------------------
# r8169 driver
#------------------------------------------------------------------------------

AC_ARG_ENABLE([r8169],
    AS_HELP_STRING([--enable-r8169],
                   [Enable r8169 driver]),
    [
        case "${enableval}" in
            yes) enable_r8169=1
                ;;
            no) enable_r8169=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-r8169])
                ;;
        esac
    ],
    [enable_r8169=0] # disabled by default
)

AM_CONDITIONAL(ENABLE_R8169, test "x$enable_r8169" = "x1")
AC_SUBST(ENABLE_R8169,[$enable_r8169])

AC_ARG_WITH([r8169-kernel],
    AC_HELP_STRING(
        [--with-r8169-kernel=<X.Y.Z>],
        [r8169 kernel (only if differing)]
    ),
    [
        kernel_r8169=[$withval]
    ],
    [
        kernel_r8169=$linuxversion
    ]
)

if test "x${enable_r8169}" = "x1"; then
    AC_MSG_CHECKING([for kernel for r8169 driver])

    kernels=`ls -1 ${srcdir}/devices/ | grep -oE "^r8169-.*-" | cut -d "-" -f 2 | uniq`
    found=0
    for k in $kernels; do
        if test "$kernel_r8169" = "$k"; then
            found=1
        fi
    done
    if test $found -ne 1; then
        AC_MSG_ERROR([kernel $kernel_r8169 not available for r8169 driver!])
    fi

    AC_MSG_RESULT([$kernel_r8169])
fi

AC_SUBST(KERNEL_R8169,[$kernel_r8169])

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
fi

AC_SUBST(RTAI_DIR,[$rtaidir])

#------------------------------------------------------------------------------
# Debug interface
#------------------------------------------------------------------------------

AC_ARG_ENABLE([debug-if],
    AS_HELP_STRING([--enable-debug-if],
                   [Create a debug interface for each master @<:@NO@:>@]),
    [
        case "${enableval}" in
            yes) dbg=1
                ;;
            no) dbg=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-debug-if])
                ;;
        esac
    ],
    [dbg=0]
)

if test "x${dbg}" = "x1"; then
    AC_DEFINE([EC_DEBUG_IF], [1], [Debug interfaces enabled])
fi
AM_CONDITIONAL(ENABLE_DEBUG_IF, test "x$dbg" = "x1")
AC_SUBST(ENABLE_DEBUG_IF,[$dbg])

#------------------------------------------------------------------------------
# Debug ring
#------------------------------------------------------------------------------

AC_ARG_ENABLE([debug-ring],
    AS_HELP_STRING([--enable-debug-ring],
                   [Create a debug ring to record frames @<:@NO@:>@]),
    [
        case "${enableval}" in
            yes) debugring=1
                ;;
            no) debugring=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-debug-ring])
                ;;
        esac
    ],
    [debugring=0]
)

if test "x${debugring}" = "x1"; then
    AC_DEFINE([EC_DEBUG_RING], [1], [Debug ring enabled])
fi

#------------------------------------------------------------------------------
# Ethernet over EtherCAT support
#------------------------------------------------------------------------------

AC_ARG_ENABLE([eoe],
    AS_HELP_STRING([--enable-eoe],
                   [Enable EoE support (default: yes)]),
    [
        case "${enableval}" in
            yes) eoe=1
                ;;
            no) eoe=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-eoe])
                ;;
        esac
    ],
    [eoe=1]
)

if test "x${eoe}" = "x1"; then
    AC_DEFINE([EC_EOE], [1], [EoE support enabled])
fi
AM_CONDITIONAL(ENABLE_EOE, test "x$eoe" = "x1")
AC_SUBST(ENABLE_EOE,[$eoe])

#------------------------------------------------------------------------------
# CPU timestamp counter support
#------------------------------------------------------------------------------

AC_ARG_ENABLE([cycles],
    AS_HELP_STRING([--enable-cycles],
                   [Use CPU timestamp counter (default: no)]),
    [
        case "${enableval}" in
            yes) cycles=1
                ;;
            no) cycles=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-cycles])
                ;;
        esac
    ],
    [cycles=0]
)

if test "x${cycles}" = "x1"; then
    AC_DEFINE([EC_HAVE_CYCLES], [1], [Use CPU timestamp counter])
fi

#------------------------------------------------------------------------------
# Command-line tool
#-----------------------------------------------------------------------------

AC_ARG_ENABLE([tool],
    AS_HELP_STRING([--enable-tool],
                   [Build command-line tool (default: yes)]),
    [
        case "${enableval}" in
            yes) tool=1
                ;;
            no) tool=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-tool])
                ;;
        esac
    ],
    [tool=1]
)

AM_CONDITIONAL(BUILD_TOOL, test "x$tool" = "x1")

#------------------------------------------------------------------------------
# Userspace library generation
#------------------------------------------------------------------------------

AC_ARG_ENABLE([userlib],
    AS_HELP_STRING([--enable-userlib],
                   [Generation of the userspace library (default: yes)]),
    [
        case "${enableval}" in
            yes) userlib=1
                ;;
            no) userlib=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-userlib])
                ;;
        esac
    ],
    [userlib=1]
)

AM_CONDITIONAL(ENABLE_USERLIB, test "x$userlib" = "x1")

#------------------------------------------------------------------------------

AC_CONFIG_FILES([
        Doxyfile
        Kbuild
        Makefile
        devices/Kbuild
        devices/Makefile
        devices/e1000/Kbuild
        devices/e1000/Makefile
        examples/Kbuild
        examples/Makefile
        examples/dc_rtai/Kbuild
        examples/dc_rtai/Makefile
        examples/dc_user/Makefile
        examples/mini/Kbuild
        examples/mini/Makefile
        examples/rtai/Kbuild
        examples/rtai/Makefile
        examples/user/Makefile
        include/Makefile
        lib/Makefile
        master/Kbuild
        master/Makefile
        script/Makefile
        script/init.d/Makefile
        script/init.d/ethercat
        script/sysconfig/Makefile
        tool/Makefile
])
AC_OUTPUT

#------------------------------------------------------------------------------
