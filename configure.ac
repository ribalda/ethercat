#------------------------------------------------------------------------------
#
#  $Id$
#
#  Copyright (C) 2006-2012  Florian Pose, Ingenieurgemeinschaft IgH
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

#
# Release procedure
# - Update Ethernet drivers up to release kernel version
# - Write NEWS entry with changes since last release
# - Check for complete Doxygen comments
# - Update version number in documentation
# - Update the ChangeLog -> hg log -bstable-1.5 --style=changelog > ChangeLog
# - Update version number below
# - make dist-bzip2
#
AC_INIT([ethercat],[1.5.2],[fp@igh-essen.com])
AC_CONFIG_AUX_DIR([autoconf])
AM_INIT_AUTOMAKE([-Wall -Werror dist-bzip2 subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])

#------------------------------------------------------------------------------
# Global
#------------------------------------------------------------------------------

AM_PROG_CC_C_O
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_CXX
AC_PROG_LIBTOOL

#------------------------------------------------------------------------------
# Kernel modules
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to build kernel modules])

AC_ARG_ENABLE([kernel],
    AS_HELP_STRING([--enable-kernel],
                   [Enable building kernel modules]),
    [
        case "${enableval}" in
            yes) enablekernel=1
                ;;
            no) enablekernel=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-generic])
                ;;
        esac
    ],
    [enablekernel=1]
)

if test "x$enablekernel" = "x1"; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(ENABLE_KERNEL, test "x$enablekernel" = "x1")
AC_SUBST(ENABLE_KERNEL,[$enablekernel])

#------------------------------------------------------------------------------
# Linux sources
#------------------------------------------------------------------------------

if test "x$enablekernel" = "x1"; then

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

if test ${kernelrelease%%.*} -gt 2; then
    regex="^[[0-9]]+\.[[0-9]]+"
else
    regex="^[[0-9]]+\.[[0-9]]+\.[[0-9]]+"
fi

# Extract numbers from kernel release
linuxversion=`echo $kernelrelease | grep -oE "$regex"`

AC_SUBST(LINUX_SOURCE_DIR,[$sourcedir])
AC_MSG_RESULT([$LINUX_SOURCE_DIR (Kernel $linuxversion)])

fi

#------------------------------------------------------------------------------
# Linux module installation subdirectory
#------------------------------------------------------------------------------

AC_ARG_WITH([module-dir],
    AC_HELP_STRING(
        [--with-module-dir=<DIR>],
        [Linux module installation dir. Default: ethercat]
    ),
    [moddir=[$withval]],
    [moddir="ethercat"]
)
AC_SUBST(INSTALL_MOD_DIR,[$moddir])

AC_MSG_CHECKING([for Linux modules installation directory])
AC_MSG_RESULT([$INSTALL_MOD_DIR])

#------------------------------------------------------------------------------
# Generic Ethernet driver
#------------------------------------------------------------------------------

AC_ARG_ENABLE([generic],
    AS_HELP_STRING([--enable-generic],
                   [Enable generic Ethernet driver]),
    [
        case "${enableval}" in
            yes) enablegeneric=1
                ;;
            no) enablegeneric=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-generic])
                ;;
        esac
    ],
    [enablegeneric=$enablekernel]
)

AM_CONDITIONAL(ENABLE_GENERIC, test "x$enablegeneric" = "x1")
AC_SUBST(ENABLE_GENERIC,[$enablegeneric])

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
    [enable8139too=$enablekernel]
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
# e1000e driver
#------------------------------------------------------------------------------

AC_ARG_ENABLE([e1000e],
    AS_HELP_STRING([--enable-e1000e],
                   [Enable e1000e driver]),
    [
        case "${enableval}" in
            yes) enablee1000e=1
                ;;
            no) enablee1000e=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-e1000e])
                ;;
        esac
    ],
    [enablee1000e=0] # disabled by default
)

AM_CONDITIONAL(ENABLE_E1000E, test "x$enablee1000e" = "x1")
AC_SUBST(ENABLE_E1000E,[$enablee1000e])

AC_ARG_WITH([e1000e-kernel],
    AC_HELP_STRING(
        [--with-e1000e-kernel=<X.Y.Z>],
        [e1000e kernel (only if differing)]
    ),
    [
        kernele1000e=[$withval]
    ],
    [
        kernele1000e=$linuxversion
    ]
)

e1000elayout=0

if test "x${enablee1000e}" = "x1"; then
    AC_MSG_CHECKING([for kernel for e1000e driver])

    kernels=`ls -1 ${srcdir}/devices/e1000e/ | grep -oE "^netdev-.*" | cut -d "-" -f 2 | uniq`
    found=0
    for k in $kernels; do
        if test "$kernele1000e" = "$k"; then
            found=1
        fi
    done
    if test $found -ne 1; then
        AC_MSG_ERROR([kernel $kernele1000e not available for e1000e driver!])
    fi

    AC_MSG_RESULT([$kernele1000e])

    # check for e1000e file layout (changed in kernel 3.4 and 3.10)
    AC_MSG_CHECKING([for e1000e source layout])

    file34="${srcdir}/devices/e1000e/80003es2lan-$kernele1000e-ethercat.c"
    file310="${srcdir}/devices/e1000e/ptp-$kernele1000e-ethercat.c"

    if test -r "$file310"; then
        AC_MSG_RESULT([>= 3.10])
        e1000elayout=2
    elif test -r "$file34"; then
        AC_MSG_RESULT([>= 3.4])
        e1000elayout=1
    else
        AC_MSG_RESULT([before 3.4])
    fi

fi

AC_SUBST(KERNEL_E1000E,[$kernele1000e])
AC_SUBST(E1000E_LAYOUT, [$e1000elayout])

#------------------------------------------------------------------------------
# igb driver
#------------------------------------------------------------------------------

AC_ARG_ENABLE([igb],
    AS_HELP_STRING([--enable-igb],
                   [Enable igb driver]),
    [
        case "${enableval}" in
            yes) enableigb=1
                ;;
            no) enableigb=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-igb])
                ;;
        esac
    ],
    [enableigb=0] # disabled by default
)

AM_CONDITIONAL(ENABLE_IGB, test "x$enableigb" = "x1")
AC_SUBST(ENABLE_IGB,[$enableigb])

AC_ARG_WITH([igb-kernel],
    AC_HELP_STRING(
        [--with-igb-kernel=<X.Y.Z>],
        [igb kernel (only if differing)]
    ),
    [
        kerneligb=[$withval]
    ],
    [
        kerneligb=$linuxversion
    ]
)

if test "x${enableigb}" = "x1"; then
    AC_MSG_CHECKING([for kernel for igb driver])

    if test ! -f "${srcdir}/devices/igb/igb_main-${kerneligb}-orig.c"; then
        AC_MSG_ERROR([kernel $kerneligb not available for igb driver!])
    fi

    AC_MSG_RESULT([$kerneligb])
fi

AC_SUBST(KERNEL_IGB,[$kerneligb])

#------------------------------------------------------------------------------
# cx2100 driver
#------------------------------------------------------------------------------

AC_ARG_ENABLE([cx2100],
    AS_HELP_STRING([--enable-cx2100],
                   [Enable cx2100 driver]),
    [
        case "${enableval}" in
            yes) enablecx2100=1
                ;;
            no) enablecx2100=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-cx2100])
                ;;
        esac
    ],
    [enablecx2100=0] # disabled by default
)

AM_CONDITIONAL(ENABLE_CX2100, test "x$enablecx2100" = "x1")
AC_SUBST(ENABLE_CX2100,[$enablecx2100])

AC_ARG_WITH([cx2100-kernel],
    AC_HELP_STRING(
        [--with-cx2100-kernel=<X.Y.Z>],
        [cx2100 kernel (only if differing)]
    ),
    [
        kernelcx2100=[$withval]
    ],
    [
        kernelcx2100=$linuxversion
    ]
)

if test "x${enablecx2100}" = "x1"; then
    AC_MSG_CHECKING([for kernel for cx2100 driver])

    kernels=`ls -1 ${srcdir}/devices/cx2100/ | grep -oE "^cx2100-.*" | cut -d "-" -f 2 | uniq`
    found=0
    for k in $kernels; do
        if test "$kernelcx2100" = "$k"; then
            found=1
        fi
    done
    if test $found -ne 1; then
        AC_MSG_ERROR([kernel $kernelcx2100 not available for cx2100 driver!])
    fi

    AC_MSG_RESULT([$kernelcx2100])
fi

AC_SUBST(KERNEL_CX2100,[$kernelcx2100])

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
# CCAT driver
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to build the CCAT driver])

AC_ARG_ENABLE([ccat],
    AS_HELP_STRING([--enable-ccat],
                   [Enable CCAT driver]),
    [
        case "${enableval}" in
            yes) enableccat=1
                ;;
            no) enableccat=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-ccat])
                ;;
        esac
    ],
    [enableccat=0] # disabled by default
)

if test "x${enableccat}" = "x1"; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(ENABLE_CCAT, test "x$enableccat" = "x1")
AC_SUBST(ENABLE_CCAT,[$enableccat])

#------------------------------------------------------------------------------
# RTAI path (optional)
#------------------------------------------------------------------------------

AC_ARG_WITH([rtai-dir],
    AC_HELP_STRING(
        [--with-rtai-dir=<DIR>],
        [RTAI path, for RTDM interface and RTAI examples]
    ),
    [
        rtaidir=[$withval]
        rtai=1
    ],
    [
        rtaidir=""
        rtai=0
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

    rtai_lxrt_cflags=`$rtaidir/bin/rtai-config --lxrt-cflags`
    rtai_lxrt_ldflags=`$rtaidir/bin/rtai-config --lxrt-ldflags`
fi

AC_SUBST(RTAI_DIR,[$rtaidir])
AM_CONDITIONAL(ENABLE_RTAI, test "x$rtai" = "x1")
AC_SUBST(ENABLE_RTAI,[$rtai])

AC_SUBST(RTAI_LXRT_CFLAGS,[$rtai_lxrt_cflags])
AC_SUBST(RTAI_LXRT_LDFLAGS,[$rtai_lxrt_ldflags])

#------------------------------------------------------------------------------
# Xenomai path (optional)
#------------------------------------------------------------------------------

AC_ARG_WITH([xenomai-dir],
    AC_HELP_STRING(
        [--with-xenomai-dir=<DIR>],
        [Xenomai path, for RTDM interface and Xenomai examples]
    ),
    [
        xenomaidir=[$withval]
        xeno=1
    ],
    [
        xenomaidir=""
        xeno=0
    ]
)

AC_MSG_CHECKING([for Xenomai path])

if test -z "${xenomaidir}"; then
    AC_MSG_RESULT([not specified.])
else
    if test \! -r ${xenomaidir}/include/xeno_config.h; then
        AC_MSG_ERROR([no Xenomai installation found in ${xenomaidir}!])
    fi
    AC_MSG_RESULT([$xenomaidir])

    xeno_native_cflags=`$xenomaidir/bin/xeno-config --skin native --cflags`
    xeno_native_ldflags=`$xenomaidir/bin/xeno-config --skin native --ldflags`
    xeno_posix_cflags=`$xenomaidir/bin/xeno-config --skin posix --cflags`
    xeno_posix_ldflags=`$xenomaidir/bin/xeno-config --skin posix --ldflags`
    xeno_rtdm_cflags=`$xenomaidir/bin/xeno-config --skin rtdm --cflags`
    xeno_rtdm_ldflags=`$xenomaidir/bin/xeno-config --skin rtdm --ldflags`
fi

AC_SUBST(XENOMAI_DIR,[$xenomaidir])
AM_CONDITIONAL(ENABLE_XENOMAI, test "x$xeno" = "x1")
AC_SUBST(ENABLE_XENOMAI,[$xeno])

AC_SUBST(XENOMAI_NATIVE_CFLAGS,[$xeno_native_cflags])
AC_SUBST(XENOMAI_NATIVE_LDFLAGS,[$xeno_native_ldflags])
AC_SUBST(XENOMAI_POSIX_CFLAGS,[$xeno_posix_cflags])
AC_SUBST(XENOMAI_POSIX_LDFLAGS,[$xeno_posix_ldflags])
AC_SUBST(XENOMAI_RTDM_CFLAGS,[$xeno_rtdm_cflags])
AC_SUBST(XENOMAI_RTDM_LDFLAGS,[$xeno_rtdm_ldflags])

#------------------------------------------------------------------------------
# RTDM interface (optional)
#------------------------------------------------------------------------------

AC_ARG_ENABLE([rtdm],
    AC_HELP_STRING(
        [--enable-rtdm],
        [Enable RTDM interface, depends on RTAI or Xenomai]
    ),
    [
        case "${enableval}" in
            yes) rtdm=1
                ;;
            no) rtdm=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-rtdm])
                ;;
        esac
    ],
    [rtdm=0]
)

AC_MSG_CHECKING([whether to build RTDM interface])

if test "x${rtdm}" = "x1"; then
    AC_DEFINE([EC_RTDM], [1], [RTDM interface enabled])
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(ENABLE_RTDM, test "x$rtdm" = "x1")
AC_SUBST(ENABLE_RTDM,[$rtdm])

#------------------------------------------------------------------------------
# Debug interface
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to build the debug interface])

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
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(ENABLE_DEBUG_IF, test "x$dbg" = "x1")
AC_SUBST(ENABLE_DEBUG_IF,[$dbg])

#------------------------------------------------------------------------------
# Debug ring
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to build the debug ring])

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
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

#------------------------------------------------------------------------------
# Ethernet over EtherCAT support
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to build with EoE support])

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
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(ENABLE_EOE, test "x$eoe" = "x1")
AC_SUBST(ENABLE_EOE,[$eoe])

#------------------------------------------------------------------------------
# CPU timestamp counter support
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to use the CPU timestamp counter])

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
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

#------------------------------------------------------------------------------
# High-resolution timer support
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to use high-resolution timers for scheduling])

AC_ARG_ENABLE([hrtimer],
    AS_HELP_STRING([--enable-hrtimer],
                   [Use high-resolution timer for scheduling (default: no)]),
    [
        case "${enableval}" in
            yes) hrtimer=1
                ;;
            no) hrtimer=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-hrtimer])
                ;;
        esac
    ],
    [hrtimer=0]
)

if test "x${hrtimer}" = "x1"; then
    AC_DEFINE([EC_USE_HRTIMER], [1], [Use hrtimer for scheduling])
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

#------------------------------------------------------------------------------
# Read alias address from register
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to read alias addresses from registers])

AC_ARG_ENABLE([regalias],
    AS_HELP_STRING([--enable-regalias],
                   [Read alias adresses from register (default: no)]),
    [
        case "${enableval}" in
            yes) regalias=1
                ;;
            no) regalias=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-regalias])
                ;;
        esac
    ],
    [regalias=0]
)

if test "x${regalias}" = "x1"; then
    AC_DEFINE([EC_REGALIAS], [1], [Read alias adresses from register])
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

#------------------------------------------------------------------------------
# Force the reference clock to OP even if unconfigured
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to force the reference clock to OP])

AC_ARG_ENABLE([refclkop],
    AS_HELP_STRING([--enable-refclkop],
                   [Force reference clock to OP (default: no)]),
    [
        case "${enableval}" in
            yes) refclkop=1
                ;;
            no) refclkop=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-refclkop])
                ;;
        esac
    ],
    [refclkop=0]
)

if test "x${refclkop}" = "x1"; then
    AC_DEFINE([EC_REFCLKOP], [1], [Force refclk to OP])
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

#------------------------------------------------------------------------------
# Command-line tool
#-----------------------------------------------------------------------------

AC_MSG_CHECKING([whether to build the command-line tool])

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

if test "x${tool}" = "x1"; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(BUILD_TOOL, test "x$tool" = "x1")

#------------------------------------------------------------------------------
# Userspace library generation
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to build the userspace library])

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

if test "x${userlib}" = "x1"; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(ENABLE_USERLIB, test "x$userlib" = "x1")

#------------------------------------------------------------------------------
# TTY driver
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to build the tty driver])

AC_ARG_ENABLE([tty],
    AS_HELP_STRING([--enable-tty],
                   [Generation of the ec_tty module (default: no)]),
    [
        case "${enableval}" in
            yes) tty=1
                ;;
            no) tty=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-tty])
                ;;
        esac
    ],
    [tty=0]
)

if test "x${tty}" = "x1"; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(ENABLE_TTY, test "x$tty" = "x1")
AC_SUBST(ENABLE_TTY,[$tty])

#------------------------------------------------------------------------------
# Slave identification wildcards
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to allow identification wildcards])

AC_ARG_ENABLE([wildcards],
    AS_HELP_STRING([--enable-wildcards],
                   [Enable vendor ID / product code wildcards (default: no)]),
    [
        case "${enableval}" in
            yes) wildcards=1
                ;;
            no) wildcards=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-wildcards])
                ;;
        esac
    ],
    [wildcards=0]
)

if test "x${wildcards}" = "x1"; then
    AC_DEFINE([EC_IDENT_WILDCARDS], [1],
        [Use vendor id / product code wildcards])
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

#------------------------------------------------------------------------------
# Redundancy (number of devices)
#------------------------------------------------------------------------------

AC_ARG_WITH([devices],
    AC_HELP_STRING(
        [--with-devices=<NUMBER>],
        [Number of Ethernet devices per master. Default: 1]
    ),
    [
        devices=[$withval]
    ],
    [
        devices=1
    ]
)

AC_MSG_CHECKING([for number of Ethernet devices])

if test "${devices}" -lt 1; then
    AC_MSG_ERROR([Number must be greater zero!])
else
    if test "${devices}" -gt 1; then
        AC_MSG_RESULT([$devices (Redundancy enabled)])
    else
        AC_MSG_RESULT([$devices (Redundancy disabled)])
    fi
fi

AC_DEFINE_UNQUOTED([EC_MAX_NUM_DEVICES], $devices,
    [Max. number of Ethernet devices per master])

#------------------------------------------------------------------------------
# SII assignment
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to assign the SII to PDI])

AC_ARG_ENABLE([sii-assign],
    AS_HELP_STRING([--enable-sii-assign],
                   [Enable SII assignment to PDI (default: yes)]),
    [
        case "${enableval}" in
            yes) siiassign=1
                ;;
            no) siiassign=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-sii-assign])
                ;;
        esac
    ],
    [siiassign=1]
)

if test "x${siiassign}" = "x1"; then
    AC_DEFINE([EC_SII_ASSIGN], [1], [Assign SII to PDI])
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

#------------------------------------------------------------------------------
# syslog output in realtime context
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to syslog in realtime context])

AC_ARG_ENABLE([rt-syslog],
    AS_HELP_STRING([--enable-rt-syslog],
                   [Enable RT syslog (default: yes)]),
    [
        case "${enableval}" in
            yes) rtsyslog=1
                ;;
            no) rtsyslog=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-rt-syslog])
                ;;
        esac
    ],
    [rtsyslog=1]
)

if test "x${rtsyslog}" = "x1"; then
    AC_DEFINE([EC_RT_SYSLOG], [1], [Output to syslog in RT context])
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

#------------------------------------------------------------------------------
# use loop control registers to open slave ports
#------------------------------------------------------------------------------

AC_MSG_CHECKING([whether to use loop control registers])

AC_ARG_ENABLE([loop-control],
    AS_HELP_STRING([--enable-loop-control],
                   [Use loop control registers (default: no)]),
    [
        case "${enableval}" in
            yes) loopctl=1
                ;;
            no) loopctl=0
                ;;
            *) AC_MSG_ERROR([Invalid value for --enable-loop-control])
                ;;
        esac
    ],
    [loopctl=0]
)

if test "x${loopctl}" = "x1"; then
    AC_DEFINE([EC_LOOP_CONTROL], [1], [Use loop control registers])
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

#------------------------------------------------------------------------------

AC_CONFIG_FILES([
        Doxyfile
        Kbuild
        Makefile
        devices/Kbuild
        devices/Makefile
        devices/ccat/Kbuild
        devices/ccat/Makefile
        devices/cx2100/Kbuild
        devices/cx2100/Makefile
        devices/e1000/Kbuild
        devices/e1000/Makefile
        devices/e1000e/Kbuild
        devices/e1000e/Makefile
        devices/igb/Kbuild
        devices/igb/Makefile
        ethercat.spec
        examples/Kbuild
        examples/Makefile
        examples/dc_rtai/Kbuild
        examples/dc_rtai/Makefile
        examples/dc_user/Makefile
        examples/mini/Kbuild
        examples/mini/Makefile
        examples/rtai/Kbuild
        examples/rtai/Makefile
        examples/rtai_rtdm/Makefile
        examples/rtai_rtdm_dc/Makefile
        examples/tty/Kbuild
        examples/tty/Makefile
        examples/user/Makefile
        examples/xenomai/Makefile
        examples/xenomai_posix/Makefile
        include/Makefile
        lib/Makefile
        m4/Makefile
        master/Kbuild
        master/Makefile
        script/Makefile
        script/ethercat.service
        script/ethercatctl
        script/init.d/Makefile
        script/init.d/ethercat
        script/sysconfig/Makefile
        tool/Makefile
        tty/Kbuild
        tty/Makefile
])
AC_OUTPUT

#------------------------------------------------------------------------------
