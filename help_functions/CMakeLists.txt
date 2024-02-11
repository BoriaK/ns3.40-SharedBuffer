set(target_prefix help_functions_)

function(create_help_functions source_files)
  # Return early if no sources in the subdirectory
  list(LENGTH source_files number_sources)
  if(number_sources EQUAL 0)
    return()
  endif()

  # If the help_functions has more than a source file, we need to find the source with
  # the main function
  set(help_functions_src)
  foreach(source_file ${source_files})
    file(READ ${source_file} source_file_contents)
    string(REGEX MATCHALL "main[(| (]" main_position "${source_file_contents}")
    if(CMAKE_MATCH_0)
      set(help_functions_src ${source_file})
    endif()
  endforeach()

  if(NOT help_functions_src)
    return()
  endif()

  # Get parent directory name
  get_filename_component(help_functions_dirname ${help_functions_src} DIRECTORY)
  string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" help_functions_dirname
                 "${help_functions_dirname}"
  )
  string(REPLACE "/" "_" help_functions_dirname "${help_functions_dirname}")

  # Get source name
  get_filename_component(help_functions_name ${help_functions_src} NAME_WE)

  set(target_prefix help_functions_)
  if(help_functions_dirname)
    # Join the names together if dirname is not the help_functions folder
    set(target_prefix help_functions${help_functions_dirname}_)
  endif()

  # Get source absolute path and transform into relative path
  get_filename_component(help_functions_src ${help_functions_src} ABSOLUTE)
  get_filename_component(help_functions_absolute_directory ${help_functions_src} DIRECTORY)
  string(REPLACE "${PROJECT_SOURCE_DIR}" "${CMAKE_OUTPUT_DIRECTORY}"
                 help_functions_directory ${help_functions_absolute_directory}
  )
  build_exec(
          EXECNAME ${help_functions_name}
          EXECNAME_PREFIX ${target_prefix}
          SOURCE_FILES "${source_files}"
          LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
          EXECUTABLE_DIRECTORY_PATH ${help_functions_directory}/
  )
endfunction()

# Scan *.cc files in ns-3-dev/help_functions and build a target for each
file(GLOB single_source_file_help_functionses CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/[^.]*.cc)
foreach(help_functions_src ${single_source_file_help_functionses})
  create_help_functions(${help_functions_src})
endforeach()

# Scan *.cc files in ns-3-dev/help_functions subdirectories and build a target for each
# subdirectory
file(
  GLOB_RECURSE help_functions_subdirectories
  CONFIGURE_DEPENDS
  LIST_DIRECTORIES true
  ${CMAKE_CURRENT_SOURCE_DIR}/**
)
# Filter out files
foreach(entry ${help_functions_subdirectories})
  if(NOT (IS_DIRECTORY ${entry}))
    list(REMOVE_ITEM help_functions_subdirectories ${entry})
  endif()
endforeach()

foreach(subdir ${help_functions_subdirectories})
  if(EXISTS ${subdir}/CMakeLists.txt)
    # If the subdirectory contains a CMakeLists.txt file
    # we let the CMake file manage the source files
    #
    # Use this if you want to link to external libraries
    # without creating a module
    add_subdirectory(${subdir})
  else()
    # Otherwise we pick all the files in the subdirectory
    # and create a help_functions for them automatically
    file(GLOB help_functions_sources CONFIGURE_DEPENDS ${subdir}/[^.]*.cc)
    create_help_functions("${help_functions_sources}")
  endif()
endforeach()
