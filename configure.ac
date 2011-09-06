# Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
AC_PREREQ(2.63)

AC_INIT([colord-playground],[0.0.1],[http://www.hughsie.com])
AC_CONFIG_SRCDIR(src)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(CLUTTER, cogl-1.0 clutter-x11-1.0 lcms2)

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT
