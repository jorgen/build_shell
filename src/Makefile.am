bin_PROGRAMS = build_shell

build_shell_SOURCES =	         	        \
	main.cpp 				\
	optionparser.h				\
	arg.h					\
	arg.cpp					\
	configuration.h				\
	configuration.cpp

AM_CPPFLAGS = $(JSON_CFLAGS) -DDATA_PATH=\"$(pkgdatadir)\"

build_shell_LDADD = $(JSON_LIBS)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = build_shell-version.h
EXTRA_DIST = build_shell-version.h.in
