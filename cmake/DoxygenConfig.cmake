find_package(Doxygen COMPONENTS dot)

function(_doxygen_common target name brief)
    set(DOC_PATHS ${ARGN})

    # Set common doxygen overrides.
    set(DOXYGEN_PROJECT_NAME  ${name})
    set(DOXYGEN_PROJECT_BRIEF ${brief})

    set(DOXYGEN_BUILTIN_STL_SUPPORT    YES)
    set(DOXYGEN_CREATE_SUBDIRS         YES)
    set(DOXYGEN_ENABLE_PREPROCESSING   NO)
    set(DOXYGEN_EXTRACT_ALL            NO)
    set(DOXYGEN_EXTRACT_ANON_NSPACES   YES)
    set(DOXYGEN_EXTRACT_STATIC         YES)
    set(DOXYGEN_GENERATE_TREEVIEW      YES)
    set(DOXYGEN_HTML_DYNAMIC_MENUS     YES)
    set(DOXYGEN_INTERACTIVE_SVG        YES)
    set(DOXYGEN_QUIET                  YES)
    set(DOXYGEN_SOURCE_BROWSER         YES)
    set(DOXYGEN_WARN_NO_PARAMDOC       YES)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)
    doxygen_add_docs(${target} "${DOC_PATHS}" ALL)
endfunction()

function(doxygen_public target name brief)
    if(DOXYGEN_FOUND)
        # Build public API docs.
        set(DOXYGEN_EXTRACT_PRIVATE      NO)
        set(DOXYGEN_INTERNAL_DOCS        NO)
        set(DOXYGEN_WARN_IF_UNDOCUMENTED NO)
        set(DOXYGEN_HIDE_UNDOC_MEMBERS   YES)
        set(DOXYGEN_HIDE_UNDOC_CLASSES   YES)
        set(DOXYGEN_OUTPUT_DIRECTORY     ${PROJECT_BINARY_DIR}/public_docs)
        _doxygen_common(${target} ${name} ${brief} "${DOXYGEN_PUBLIC_PATHS}")
    endif()
endfunction()

function(doxygen_internal target name brief)
    if(DOXYGEN_FOUND)
        # Build internal reference docs.
        set(DOXYGEN_EXTRACT_PRIVATE      YES)
        set(DOXYGEN_INTERNAL_DOCS        YES)
        set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
        set(DOXYGEN_HIDE_UNDOC_MEMBERS   NO)
        set(DOXYGEN_HIDE_UNDOC_CLASSES   NO)
        set(DOXYGEN_OUTPUT_DIRECTORY     ${PROJECT_BINARY_DIR}/internal_docs)
        _doxygen_common(${target} ${name} ${brief} "${DOXYGEN_INTERNAL_PATHS}")
    endif()
endfunction()

macro(_update_path path_var new_paths)
    set(paths "${${path_var}}")
    list(APPEND paths "${new_paths}")
    list(REMOVE_DUPLICATES paths)
    set(${path_var} "${paths}" CACHE INTERNAL "doxygen paths" FORCE)
endmacro()

function(document)
    cmake_parse_arguments(ARG "PUBLIC;INTERNAL" "" "" ${ARGN})
    set(paths "${ARG_UNPARSED_ARGUMENTS}")
    list(TRANSFORM paths PREPEND ${CMAKE_CURRENT_LIST_DIR}/)

    if (NOT ${ARG_PUBLIC})
        _update_path(DOXYGEN_INTERNAL_PATHS "${paths}")
    endif()

    if (NOT ${ARG_INTERNAL})
        _update_path(DOXYGEN_PUBLIC_PATHS "${paths}")
    endif()
endfunction()
