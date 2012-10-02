
# (ADD_PCH_RULE  _header_filename _src_list)
# Version 7/26/2010 4:55pm
#
# use this macro before "add_executable"
#
# _header_filename
#	header to make a .gch
#
# _src_list
#   the variable name (do not use ${..}) which contains a
#     a list of sources (a.cpp b.cpp c.cpp ...)
#  This macro will append a header file to it, then this src_list can be used in
#	"add_executable..."
#
#
# Now a .gch file should be generated and gcc should use it.
#       	(add -Winvalid-pch to the cpp flags to verify)
#
# make clean should delete the pch file
#
# example : ADD_PCH_RULE(headers.h myprog_SRCS)

IF(CMAKE_COMPILER_IS_GNUCXX)

    EXEC_PROGRAM(
    	${CMAKE_CXX_COMPILER}
        ARGS 	${CMAKE_CXX_COMPILER_ARG1} -dumpversion
        OUTPUT_VARIABLE gcc_compiler_version)
    #MESSAGE("GCC Version: ${gcc_compiler_version}")
    IF(gcc_compiler_version MATCHES "4\\.[0-9]\\.[0-9]")
        SET(PCHSupport_FOUND TRUE)
    ELSE(gcc_compiler_version MATCHES "4\\.[0-9]\\.[0-9]")
        IF(gcc_compiler_version MATCHES "3\\.4\\.[0-9]")
            SET(PCHSupport_FOUND TRUE)
        ENDIF(gcc_compiler_version MATCHES "3\\.4\\.[0-9]")
    ENDIF(gcc_compiler_version MATCHES "4\\.[0-9]\\.[0-9]")

	SET(_PCH_include_prefix "-I")

ELSE(CMAKE_COMPILER_IS_GNUCXX)
	IF(WIN32)
		SET(PCHSupport_FOUND FALSE) # no msvc support yet...
		SET(_PCH_include_prefix "/I")
	ELSE(WIN32)
		SET(PCHSupport_FOUND FALSE)
	ENDIF(WIN32)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

# ----

MACRO(_PCH_GET_COMPILE_FLAGS _out_compile_flags)
	STRING(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" _flags_var_name)
	SET(${_out_compile_flags} ${CMAKE_CXX_COMPILER_ARG1})

	LIST(APPEND ${_out_compile_flags} ${${_flags_var_name}})

	GET_DIRECTORY_PROPERTY(DIRINC INCLUDE_DIRECTORIES)
	FOREACH(item ${DIRINC})
	LIST(APPEND ${_out_compile_flags} "${_PCH_include_prefix} ${item}")
	ENDFOREACH(item)

	GET_DIRECTORY_PROPERTY(_directory_flags DEFINITIONS)
	LIST(APPEND ${_out_compile_flags} ${_directory_flags})
	LIST(APPEND ${_out_compile_flags} ${CMAKE_CXX_FLAGS})

	# Individual libraries' quirks
	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		if (QT_FOUND)
			LIST(APPEND ${_out_compile_flags} "-DQT_DEBUG")
		endif (QT_FOUND)
	endif("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")

	SEPARATE_ARGUMENTS(${_out_compile_flags})
ENDMACRO(_PCH_GET_COMPILE_FLAGS)

macro(GCC_ADD_GCH _header gch_file)
	set(${gch_file} "${header}.gch")
	_PCH_GET_COMPILE_FLAGS(gch_flags)
	set(io_spec -c ${header} -o ${${gch_file}})

	add_custom_command(	OUTPUT ${${gch_file}}
						COMMAND rm -f ${${gch_file}}
						COMMAND ${CMAKE_CXX_COMPILER} ${gch_flags} ${io_spec}
						MAIN_DEPENDENCY ${header}
						DEPENDS ${header}
						IMPLICIT_DEPENDS CXX ${header})
endmacro(GCC_ADD_GCH)

macro(MAIN_ADD_GCH _header sources)
	get_source_file_property(header ${_header} LOCATION)

	if (PCHSupport_FOUND)
		if (CMAKE_COMPILER_IS_GNUCXX)
			GCC_ADD_GCH (header out_file)
		else (CMAKE_COMPILER_IS_GNUCXX)
			# Not ready yet...
		endif (CMAKE_COMPILER_IS_GNUCXX)

		# The dependency
		set_source_files_properties(${sources} PROPERTIES OBJECT_DEPENDS "${out_file}")
	endif (PCHSupport_FOUND)
endmacro(MAIN_ADD_GCH)