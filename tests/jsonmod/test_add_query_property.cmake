if (NOT jsonmod_exec)
    message(FATAL_ERROR "Variable jsonmod_exec not defined")
endif (NOT jsonmod_exec)

if (NOT property)
    message(FATAL_ERROR "Variable property not defined")
endif (NOT property)

if (NOT value)
    message(FATAL_ERROR "Variable value not defined")
endif (NOT value)

if (NOT input_file)
    message(FATAL_ERROR "Variable input_file not defined")
endif (NOT input_file)

if (NOT test_name)
    message(FATAL_ERROR "Variable test_name not defined")
endif (NOT test_name)

set(TEST_OUT ${test_name}.out)
set(TEST_EXPECTED ${input_file}.expected)

execute_process(
    COMMAND ${jsonmod_exec} -p ${property} -v ${value} ${input_file}
    OUTPUT_FILE ${TEST_OUT}
)

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${TEST_OUT} ${TEST_EXPECTED}
    RESULT_VARIABLE files_not_equal)

if( files_not_equal )
    message( FATAL_ERROR "Files are not equal" )
endif( files_not_equal )
