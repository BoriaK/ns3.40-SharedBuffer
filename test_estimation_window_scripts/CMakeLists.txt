set(target_prefix test_estimation_window_)

function(create_test_estimation_window source_files)
  # Return early if no sources in the subdirectory
  list(LENGTH source_files number_sources)
  if(number_sources EQUAL 0)
    return()
  endif()

  # If the test_estimation_window has more than a source file, we need to find the source with
  # the main function
  set(test_estimation_window_src)
  foreach(source_file ${source_files})
    file(READ ${source_file} source_file_contents)
    string(REGEX MATCHALL "main[(| (]" main_position "${source_file_contents}")
    if(CMAKE_MATCH_0)
      set(test_estimation_window_src ${source_file})
    endif()
  endforeach()

  if(NOT test_estimation_window_src)
    return()
  endif()

  # Get parent directory name
  get_filename_component(test_estimation_window_dirname ${test_estimation_window_src} DIRECTORY)
  string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" test_estimation_window_dirname
                 "${test_estimation_window_dirname}"
  )
  string(REPLACE "/" "_" test_estimation_window_dirname "${test_estimation_window_dirname}")

  # Get source name
  get_filename_component(test_estimation_window_name ${test_estimation_window_src} NAME_WE)

  set(target_prefix test_estimation_window_)
  if(test_estimation_window_dirname)
    # Join the names together if dirname is not the test_estimation_window folder
    set(target_prefix test_estimation_window${test_estimation_window_dirname}_)
  endif()

  # Get source absolute path and transform into relative path
  get_filename_component(test_estimation_window_src ${test_estimation_window_src} ABSOLUTE)
  get_filename_component(test_estimation_window_absolute_directory ${test_estimation_window_src} DIRECTORY)
  string(REPLACE "${PROJECT_SOURCE_DIR}" "${CMAKE_OUTPUT_DIRECTORY}"
                 test_estimation_window_directory ${test_estimation_window_absolute_directory}
  )
  build_exec(
          EXECNAME ${test_estimation_window_name}
          EXECNAME_PREFIX ${target_prefix}
          SOURCE_FILES "${source_files}"
          LIBRARIES_TO_LINK "${ns3-libs}" "${ns3-contrib-libs}"
          EXECUTABLE_DIRECTORY_PATH ${test_estimation_window_directory}/
  )
endfunction()

# Scan *.cc files in ns-3-dev/test_estimation_window and build a target for each
file(GLOB single_source_file_test_estimation_windowes CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/[^.]*.cc)
foreach(test_estimation_window_src ${single_source_file_test_estimation_windowes})
  create_test_estimation_window(${test_estimation_window_src})
endforeach()

# Scan *.cc files in ns-3-dev/test_estimation_window subdirectories and build a target for each
# subdirectory
file(
  GLOB_RECURSE test_estimation_window_subdirectories
  CONFIGURE_DEPENDS
  LIST_DIRECTORIES true
  ${CMAKE_CURRENT_SOURCE_DIR}/**
)
# Filter out files
foreach(entry ${test_estimation_window_subdirectories})
  if(NOT (IS_DIRECTORY ${entry}))
    list(REMOVE_ITEM test_estimation_window_subdirectories ${entry})
  endif()
endforeach()

foreach(subdir ${test_estimation_window_subdirectories})
  if(EXISTS ${subdir}/CMakeLists.txt)
    # If the subdirectory contains a CMakeLists.txt file
    # we let the CMake file manage the source files
    #
    # Use this if you want to link to external libraries
    # without creating a module
    add_subdirectory(${subdir})
  else()
    # Otherwise we pick all the files in the subdirectory
    # and create a test_estimation_window for them automatically
    file(GLOB test_estimation_window_sources CONFIGURE_DEPENDS ${subdir}/[^.]*.cc)
    create_test_estimation_window("${test_estimation_window_sources}")
  endif()
endforeach()
