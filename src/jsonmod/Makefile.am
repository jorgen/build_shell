bin_PROGRAMS = jsonmod

jsonmod_SOURCES =   	        					\
	main.cpp 							\
	arg.h								\
	arg.cpp								\
	configuration.h							\
	configuration.cpp						\
	json_streamer.cpp						\
	$(top_srcdir)/src/3rdparty/json_tools/src/json_tokenizer.cpp

AM_CPPFLAGS = \
    -I$(top_srcdir)/src/3rdparty/json_tools/src \
    -I$(top_srcdir)/src/3rdparty/optionparser/src
#AM_CPPFLAGS =  -DDATA_PATH=\"$(pkgdatadir)\"

CLEANFILES = $(BUILT_SOURCES)
