AC_PREREQ([2.64])

m4_define([build_shell_major_version], [0])
m4_define([build_shell_minor_version], [95])
m4_define([build_shell_micro_version], [0])
m4_define([build_shell_version],
          [build_shell_major_version.build_shell_minor_version.build_shell_micro_version])

AC_INIT([build_shell],
        [build_shell_version],
        [https://github.com/jorgen/build_shell],
        [build_shell],
        [https://github.com/jorgen/build_shell])

AC_SUBST([BUILD_SHELL_VERSION_MAJOR], [build_shell_major_version])
AC_SUBST([BUILD_SHELL_VERSION_MINOR], [build_shell_minor_version])
AC_SUBST([BUILD_SHELL_VERSION_MICRO], [build_shell_micro_version])
AC_SUBST([BUILD_SHELL_VERSION], [build_shell_version])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])

AM_SILENT_RULES([yes])

AC_LANG(C++)
AC_PROG_CXX

m4_include([m4/ax_cxx_compile_stdcxx_11.m4])

AX_CXX_COMPILE_STDCXX_11([noext])

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(JSON, [json])

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Wall -Wextra -Wno-unused-parameter -g -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

#AC_CHECK_FUNCS([accept4 mkostemp])

#AC_ARG_ENABLE([scanner],
#              [AC_HELP_STRING([--disable-scanner],
#                              [Disable compilation of wayland-scannner])],
#              [],
#              [enable_scanner=yes])
#
#AC_ARG_ENABLE([documentation],
#	      [AC_HELP_STRING([--disable-documentation],
#		              [Disable building the documentation])],
#	      [],
#	      [enable_documentation=yes])

#AM_CONDITIONAL(ENABLE_SCANNER, test "x$enable_scanner" = xyes)

#AC_ARG_WITH(icondir, [  --with-icondir=<dir>    Look for cursor icons here],
#		     [  ICONDIR=$withval],
#		     [  ICONDIR=${datadir}/icons])
#AC_SUBST([ICONDIR])

#EXPAT_LIB=""
#AC_ARG_WITH(expat, [  --with-expat=<dir>      Use expat from here],
#		   [ expat=$withval
#		     CPPFLAGS="$CPPFLAGS -I$withval/include"
#		     LDFLAGS="$LDFLAGS -L$withval/lib" ] )

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 src/build_shell-version.h])
AC_OUTPUT
