set(target_prefix execution_scripts_)

function(create_execution_scripts source_files)
  # Return early if no sources in the subdirectory
  list(LENGTH source_files number_sources)
  if(number_sources EQUAL 0)
    return()
  endif()

  # If the execution_scripts has more than a source file, we need to find the source with
  # the main function
  set(execution_scripts_src)
  foreach(source_file ${source_files})
    file(READ ${source_file} source_file_contents)
    string(REGEX MATCHALL "main[(| (]" main_position "${source_file_contents}")
    if(CMAKE_MATCH_0)
      set(execution_scripts_src ${source_file})
    endif()
  endforeach()

  if(NOT execution_scripts_src)
    return()
  endif()

  # Get parent directory name
  get_filename_component(execution_scripts_dirname ${execution_scripts_src} DIRECTORY)
  string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" execution_scripts_dirname
                 "${execution_scripts_dirname}"
  )
  string(REPLACE "/" "_" execution_scripts_dirname "${execution_scripts_dirname}")

  # Get source name
  get_filename_component(execution_scripts_name ${execution_scripts_src} NAME_WE)

  set(target_prefix execution_scripts_)
  if(execution_scripts_dirname)
    # Join the names together if dirname is not the execution_scripts folder
    set(target_prefix execution_scripts${execution_scripts_dirname}_)
  endif()

  # Get source absolute path and transform into relative path
  get_filename_component(execution_scripts_src ${execution_scripts_src} ABSOLUTE)
  get_filename_component(execution_scripts_absolute_directory ${execution_scripts_src} DIRECTORY)
  string(REPLACE "${PROJECT_SOURCE_DIR}" "${CMAKE_OUTPUT_DIRECTORY}"
                 execution_scripts_directory ${execution_scripts_absolute_directory}
  )
  build_exec(
          EXECNAME ${execution_scripts_name}
          EXECNAME_PREFIX ${target_prefix}
          SOURCE_FILES "${source_files}"
          LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
          EXECUTABLE_DIRECTORY_PATH ${execution_scripts_directory}/
  )
endfunction()

# Scan *.cc files in ns-3-dev/execution_scripts and build a target for each
file(GLOB single_source_file_execution_scriptses CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/[^.]*.cc)
foreach(execution_scripts_src ${single_source_file_execution_scriptses})
  create_execution_scripts(${execution_scripts_src})
endforeach()

# Scan *.cc files in ns-3-dev/execution_scripts subdirectories and build a target for each
# subdirectory
file(
  GLOB_RECURSE execution_scripts_subdirectories
  CONFIGURE_DEPENDS
  LIST_DIRECTORIES true
  ${CMAKE_CURRENT_SOURCE_DIR}/**
)
# Filter out files
foreach(entry ${execution_scripts_subdirectories})
  if(NOT (IS_DIRECTORY ${entry}))
    list(REMOVE_ITEM execution_scripts_subdirectories ${entry})
  endif()
endforeach()

foreach(subdir ${execution_scripts_subdirectories})
  if(EXISTS ${subdir}/CMakeLists.txt)
    # If the subdirectory contains a CMakeLists.txt file
    # we let the CMake file manage the source files
    #
    # Use this if you want to link to external libraries
    # without creating a module
    add_subdirectory(${subdir})
  else()
    # Otherwise we pick all the files in the subdirectory
    # and create a execution_scripts for them automatically
    file(GLOB execution_scripts_sources CONFIGURE_DEPENDS ${subdir}/[^.]*.cc)
    create_execution_scripts("${execution_scripts_sources}")
  endif()
endforeach()
