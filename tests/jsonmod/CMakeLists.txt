#file(GLOB ADD_PROPERTY_FILES ${CMAKE_CURRENT_SOURCE_DIR}/add_property*)
#foreach(ADD_PROPERTY_FILE ${ADD_PROPERTY_FILES})
#    configure_file(${ADD_PROPERTY_FILE} ${ADD_PROPERTY_FILE} COPYONLY)
#endforeach(ADD_PROPERTY_FILE)
#add_test(AddProperty add_property WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
#add_test(NAME OnlyModifySpecifiedSubtree COMMAND only_modify_specified_subtree)

add_test( "add_property"
 ${CMAKE_COMMAND}
 -Djsonmod_exec=${CMAKE_BINARY_DIR}/src/jsonmod/jsonmod
 -Dproperty=qtjsbackend.new_property
 -Dvalue=new_value
 -Dinput_file=${CMAKE_CURRENT_SOURCE_DIR}/add_property.json
 -Dtest_name=add_property
 -P ${CMAKE_CURRENT_SOURCE_DIR}/test_add_query_property.cmake
 )

add_test( "only_modify_specified_subtree"
 ${CMAKE_COMMAND}
 -Djsonmod_exec=${CMAKE_BINARY_DIR}/src/jsonmod/jsonmod
 -Dproperty=qtjsbackend.scm.branch
 -Dvalue=new_branch
 -Dinput_file=${CMAKE_CURRENT_SOURCE_DIR}/only_modify_specified_subtree.json
 -Dtest_name=only_modify_specified_subtree
 -P ${CMAKE_CURRENT_SOURCE_DIR}/test_add_query_property.cmake
 )
