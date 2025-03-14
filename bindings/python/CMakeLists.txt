if(UNIX)
  set(PYTHON_COMPONENTS Development.Module)
endif()

include(../../cmake-module/python.cmake)
include(../../cmake-module/python-helpers.cmake)

findpython(REQUIRED Development.Module)

if(IS_ABSOLUTE ${PYTHON_SITELIB})
  set(${PYWRAP}_INSTALL_DIR ${PYTHON_SITELIB}/${PROJECT_NAME})
else()
  set(${PYWRAP}_INSTALL_DIR
      ${CMAKE_INSTALL_PREFIX}/${PYTHON_SITELIB}/${PROJECT_NAME})
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/external/pybind11)
add_subdirectory(external/pybind11
                 ${CMAKE_CURRENT_BINARY_DIR}/external/pybind11)

add_custom_target(python)

# Collect files
file(GLOB_RECURSE PYWRAP_HEADERS ${CMAKE_CURRENT_LIST_DIR}/src/*.hpp)
file(GLOB_RECURSE PYWRAP_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)

# Add simd feature detectors for current intel CPU
if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
  python3_add_library(instructionset MODULE helpers/instruction-set.cpp)
  add_dependencies(python instructionset)
  target_link_libraries(instructionset PRIVATE proxsuite pybind11::module)
  set_target_properties(
    instructionset
    PROPERTIES OUTPUT_NAME instructionset
               LIBRARY_OUTPUT_DIRECTORY
               "${CMAKE_BINARY_DIR}/bindings/python/${PROJECT_NAME}"
               LIBRARY_OUTPUT_DIRECTORY_RELEASE
               "${CMAKE_BINARY_DIR}/bindings/python/${PROJECT_NAME}"
               LIBRARY_OUTPUT_DIRECTORY_DEBUG
               "${CMAKE_BINARY_DIR}/bindings/python/${PROJECT_NAME}")
  if(UNIX AND NOT APPLE)
    set_target_properties(instructionset PROPERTIES INSTALL_RPATH
                                                    "\$ORIGIN/../../..")
  endif()
  install(
    TARGETS instructionset
    EXPORT ${TARGETS_EXPORT_NAME}
    DESTINATION ${${PYWRAP}_INSTALL_DIR})
endif()

function(list_filter list regular_expression dest_list)
  foreach(elt ${list})
    if(${elt} MATCHES ${regular_expression})
      list(REMOVE_ITEM list ${elt})
    endif()
  endforeach(elt ${list})
  set(${dest_list}
      ${list}
      PARENT_SCOPE)
endfunction(list_filter)

function(CREATE_PYTHON_TARGET target_name COMPILE_OPTIONS dependencies)
  python3_add_library(${target_name} MODULE ${PYWRAP_SOURCES} ${PYWRAP_HEADERS})
  add_dependencies(python ${target_name})

  target_link_libraries(${target_name} PUBLIC ${dependencies} pybind11::module)
  target_compile_options(${target_name} PRIVATE ${COMPILE_OPTIONS})
  target_link_libraries(${target_name} PRIVATE proxsuite pybind11::module)
  target_compile_definitions(${target_name}
                             PRIVATE PYTHON_MODULE_NAME=${target_name})

  if(BUILD_WITH_OPENMP_SUPPORT)
    target_compile_options(${target_name} PRIVATE ${OpenMP_CXX_FLAGS})
    target_compile_definitions(${target_name}
                               PRIVATE -DPROXSUITE_PYTHON_INTERFACE_WITH_OPENMP)
    target_include_directories(${target_name} SYSTEM
                               PRIVATE ${OpenMP_CXX_INCLUDE_DIR})
    if(LINK_PYTHON_INTERFACE_TO_OPENMP)
      target_link_libraries(${target_name} PRIVATE ${OpenMP_CXX_LIBRARIES})
    endif(LINK_PYTHON_INTERFACE_TO_OPENMP)
  else(BUILD_WITH_OPENMP_SUPPORT)
    list_filter("${PYWRAP_HEADERS}" "expose-parallel" PYWRAP_HEADERS)
  endif(BUILD_WITH_OPENMP_SUPPORT)

  target_include_directories(
    ${target_name} SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/external/cereal/include)
  set_target_properties(
    ${target_name}
    PROPERTIES OUTPUT_NAME ${target_name}
               LIBRARY_OUTPUT_DIRECTORY
               "${CMAKE_BINARY_DIR}/bindings/python/${PROJECT_NAME}"
               LIBRARY_OUTPUT_DIRECTORY_RELEASE
               "${CMAKE_BINARY_DIR}/bindings/python/${PROJECT_NAME}"
               LIBRARY_OUTPUT_DIRECTORY_DEBUG
               "${CMAKE_BINARY_DIR}/bindings/python/${PROJECT_NAME}")

  if(UNIX AND NOT APPLE)
    set_target_properties(${target_name} PROPERTIES INSTALL_RPATH
                                                    "\$ORIGIN/../../..")
  endif()

  install(TARGETS ${target_name} DESTINATION ${${PYWRAP}_INSTALL_DIR})
endfunction()

if(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
  set(AVX_COMPILE_OPTION "/arch:AVX")
  set(AVX2_COMPILE_OPTION "/arch:AVX2")
  set(FMA_COMPILE_OPTION "/fp:fast,")
  set(AVX512_COMPILE_OPTION "/arch:AVX512")
else()
  set(AVX_COMPILE_OPTION "-mavx")
  set(AVX2_COMPILE_OPTION "-mavx2")
  set(FMA_COMPILE_OPTION "-mfma")
  set(AVX512_COMPILE_OPTION "-mavx512f")
endif()

create_python_target(proxsuite_pywrap "" proxsuite)
if(BUILD_WITH_VECTORIZATION_SUPPORT AND ${CMAKE_SYSTEM_PROCESSOR} MATCHES
                                        "(x86)|(X86)|(amd64)|(AMD64)")
  if(BUILD_BINDINGS_WITH_AVX2_SUPPORT)
    create_python_target(
      proxsuite_pywrap_avx2 "${AVX2_COMPILE_OPTION};${FMA_COMPILE_OPTION}"
      proxsuite-vectorized)
  endif(BUILD_BINDINGS_WITH_AVX2_SUPPORT)
  if(BUILD_BINDINGS_WITH_AVX512_SUPPORT)
    create_python_target(
      proxsuite_pywrap_avx512 "${AVX512_COMPILE_OPTION};${FMA_COMPILE_OPTION}"
      proxsuite-vectorized)
  endif(BUILD_BINDINGS_WITH_AVX512_SUPPORT)
endif()
add_header_group(PYWRAP_HEADERS)
add_source_group(PYWRAP_SOURCES)

# --- INSTALL SCRIPTS

# On Windows, we need to enforce the environment variable KMP_DUPLICATE_LIB_OK
# to True to to allow the program to continue to execute with OpenMP support
if(WIN32
   AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND BUILD_WITH_OPENMP_SUPPORT)
  set(OPENMP_KMP_DUPLICATE_LIB_OK_SCRIPT
      "import os\n" "os.environ[\"KMP_DUPLICATE_LIB_OK\"] = \"1\"\n\n")

  if(CMAKE_GENERATOR MATCHES "Visual Studio|Xcode")
    set(PYTHON_MODULE_DIR "${CMAKE_CURRENT_BINARY_DIR}/proxsuite/$<CONFIG>")
  else()
    set(PYTHON_MODULE_DIR "${CMAKE_CURRENT_BINARY_DIR}/proxsuite")
  endif()

  set(original_init_dot_py_file ${CMAKE_CURRENT_LIST_DIR}/proxsuite/__init__.py)
  set(generated_init_dot_py_file
      ${CMAKE_CURRENT_BINARY_DIR}/proxsuite/__init__.py)
  set(generated_init_dot_pyc_file ${PYTHON_MODULE_DIR}/__init__.pyc)

  # Copy content of the __init__.py file
  file(READ ${original_init_dot_py_file} INIT_CONTENT)
  # Create a new __init__.py file containing both the content of __init__.py
  # prepended with the OPENMP_KMP_DUPLICATE_LIB_OK_SCRIPT content
  file(WRITE ${generated_init_dot_py_file}
       ${OPENMP_KMP_DUPLICATE_LIB_OK_SCRIPT})
  file(APPEND ${generated_init_dot_py_file} ${INIT_CONTENT})

  python_build_file(${generated_init_dot_py_file}
                    ${generated_init_dot_pyc_file})
  install(FILES "${generated_init_dot_py_file}"
          DESTINATION ${${PYWRAP}_INSTALL_DIR})

else()
  python_build(${PROJECT_NAME} __init__.py)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/proxsuite/__init__.py"
          DESTINATION ${${PYWRAP}_INSTALL_DIR})
endif()

python_build_get_target(compile_pyc)
add_dependencies(python ${compile_pyc})

set(PYTHON_FILES torch/__init__.py torch/qplayer.py torch/utils.py)

file(MAKE_DIRECTORY ${${PYWRAP}_INSTALL_DIR}/torch)

foreach(python ${PYTHON_FILES})
  python_build(${PROJECT_NAME} ${python})
  get_filename_component(pysubmodule ${python} PATH)
  get_filename_component(pyname ${python} NAME)
  set(MODULE_NAME ${PROJECT_NAME}/${pysubmodule})
  python_install_on_site(${MODULE_NAME} ${pyname})
endforeach(python)
