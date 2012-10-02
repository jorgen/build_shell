bin_PROGRAMS = build_shell

build_shell_SOURCES =	         	        \
	main.cpp 				\
	arg.h					\
	arg.cpp					\
	configuration.h				\
	configuration.cpp			\
	witer.h					\
	writer.cpp				\
	3rdparty/jsmn/jsmn.c

AM_CPPFLAGS = $(JSON_CFLAGS) -DDATA_PATH=\"$(pkgdatadir)\"

build_shell_LDADD = $(JSON_LIBS)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = build_shell-version.h
EXTRA_DIST = build_shell-version.h.in
