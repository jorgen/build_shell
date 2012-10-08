bin_PROGRAMS = build_shell

build_shell_SOURCES =	         	        \
	main.cpp 				\
	arg.h					\
	arg.cpp					\
	configuration.h				\
	configuration.cpp			\
	witer.h					\
	writer.cpp				\
	creator.h				\
	creator.cpp				\
	../3rdparty/jsmn/jsmn.c

AM_CPPFLAGS = -DDATA_PATH=\"$(pkgdatadir)\"

CLEANFILES = $(BUILT_SOURCES)
