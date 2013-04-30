bin_PROGRAMS = build_shell

build_shell_SOURCES =	         	        \
	main.cpp 				\
	arg.h					\
	arg.cpp					\
	configuration.h				\
	configuration.cpp			\
	witer.h					\
	writer.cpp				\
	action.h				\
	action.cpp				\
	create_action.h				\
	create_action.cpp


if ENABLE_DEVELOPER_BUILD
    AM_CPPFLAGS = -DSCRIPTS_PATH=\"$(abs_top_srcdir)/data/build_shell/scripts\"
else
    AM_CPPFLAGS = -DSCRIPTS_PATH=\"$(pkgdatadir)/build_shell/scripts\"
endif

CLEANFILES = $(BUILT_SOURCES)
