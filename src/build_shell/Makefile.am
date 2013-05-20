bin_PROGRAMS = build_shell

build_shell_SOURCES =	         	        			\
	main.cpp 							\
	arg.h								\
	arg.cpp								\
	configuration.h							\
	configuration.cpp						\
	witer.h								\
	writer.cpp							\
	action.h							\
	action.cpp							\
	create_action.h							\
	create_action.cpp						\
	$(top_srcdir)/src/3rdparty/json_tools/src/json_tokenizer.cpp 	\
	$(top_srcdir)/src/3rdparty/json_tools/src/json_tree.cpp		\
	mmapped_file.cpp						\
	mmapped_file.h							\
	pull_action.cpp							\
	pull_action.h							\
	tree_builder.cpp						\
	tree_builder.h							\
	tree_writer.cpp							\
	tree_writer.h							\
	build_action.cpp						\
	build_action.h							\
	env_script_builder.cpp						\
	env_script_builder.h						\
	available_builds.cpp					        \
	available_builds.h						\
	temp_file.cpp							\
	temp_file.h   							\
	child_process_io_handler.cpp					\
	child_process_io_handler.h



AM_CPPFLAGS = \
    -I$(top_srcdir)/src/3rdparty/json_tools/src \
    -I$(top_srcdir)/src/3rdparty/optionparser/src

if ENABLE_DEVELOPER_BUILD
    AM_CPPFLAGS += -DSCRIPTS_PATH=\"$(abs_top_srcdir)/data/build_shell/scripts\"
    AM_CPPFLAGS += -DJSONMOD_PATH=\"$(abs_top_srcdir)/src/jsonmod\"
else
    AM_CPPFLAGS += -DSCRIPTS_PATH=\"$(pkgdatadir)/build_shell/scripts\"
endif

CLEANFILES = $(BUILT_SOURCES)
