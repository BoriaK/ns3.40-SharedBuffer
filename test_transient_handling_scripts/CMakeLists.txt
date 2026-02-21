set(target_prefix test_transient_handling_)

function(create_test_transient_handling source_files)
  # Return early if no sources in the subdirectory
  list(LENGTH source_files number_sources)
  if(number_sources EQUAL 0)
    return()
  endif()

  # If the test_transient_handling has more than a source file, we need to find the source with
  # the main function
  set(test_transient_handling_src)
  foreach(source_file ${source_files})
    file(READ ${source_file} source_file_contents)
    string(REGEX MATCHALL "main[(| (]" main_position "${source_file_contents}")
    if(CMAKE_MATCH_0)
      set(test_transient_handling_src ${source_file})
    endif()
  endforeach()

  if(NOT test_transient_handling_src)
    return()
  endif()

  # Get parent directory name
  get_filename_component(test_transient_handling_dirname ${test_transient_handling_src} DIRECTORY)
  string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" test_transient_handling_dirname
                 "${test_transient_handling_dirname}"
  )
  string(REPLACE "/" "_" test_transient_handling_dirname "${test_transient_handling_dirname}")

  # Get source name
  get_filename_component(test_transient_handling_name ${test_transient_handling_src} NAME_WE)

  set(target_prefix test_transient_handling_)
  if(test_transient_handling_dirname)
    # Join the names together if dirname is not the test_transient_handling folder
    set(target_prefix test_transient_handling${test_transient_handling_dirname}_)
  endif()

  # Get source absolute path and transform into relative path
  get_filename_component(test_transient_handling_src ${test_transient_handling_src} ABSOLUTE)
  get_filename_component(test_transient_handling_absolute_directory ${test_transient_handling_src} DIRECTORY)
  string(REPLACE "${PROJECT_SOURCE_DIR}" "${CMAKE_OUTPUT_DIRECTORY}"
                 test_transient_handling_directory ${test_transient_handling_absolute_directory}
  )
  build_exec(
          EXECNAME ${test_transient_handling_name}
          EXECNAME_PREFIX ${target_prefix}
          SOURCE_FILES "${source_files}"
          LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
          EXECUTABLE_DIRECTORY_PATH ${test_transient_handling_directory}/
  )
endfunction()

# Scan *.cc files in ns-3-dev/test_transient_handling and build a target for each
file(GLOB single_source_file_test_transient_handlinges CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/[^.]*.cc)
foreach(test_transient_handling_src ${single_source_file_test_transient_handlinges})
  create_test_transient_handling(${test_transient_handling_src})
endforeach()

# Scan *.cc files in ns-3-dev/test_transient_handling subdirectories and build a target for each
# subdirectory
file(
  GLOB_RECURSE test_transient_handling_subdirectories
  CONFIGURE_DEPENDS
  LIST_DIRECTORIES true
  ${CMAKE_CURRENT_SOURCE_DIR}/**
)
# Filter out files
foreach(entry ${test_transient_handling_subdirectories})
  if(NOT (IS_DIRECTORY ${entry}))
    list(REMOVE_ITEM test_transient_handling_subdirectories ${entry})
  endif()
endforeach()

foreach(subdir ${test_transient_handling_subdirectories})
  if(EXISTS ${subdir}/CMakeLists.txt)
    # If the subdirectory contains a CMakeLists.txt file
    # we let the CMake file manage the source files
    #
    # Use this if you want to link to external libraries
    # without creating a module
    add_subdirectory(${subdir})
  else()
    # Otherwise we pick all the files in the subdirectory
    # and create a test_transient_handling for them automatically
    file(GLOB test_transient_handling_sources CONFIGURE_DEPENDS ${subdir}/[^.]*.cc)
    create_test_transient_handling("${test_transient_handling_sources}")
  endif()
endforeach()
