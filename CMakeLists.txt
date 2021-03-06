cmake_minimum_required(VERSION 3.6) # Intel C++11 support starts from 3.6 or even later version
project(SIRIUS)

# set language and standard
enable_language(CXX Fortran)
set(CMAKE_CXX_STANDARD 11)

# user variables
set(CREATE_PYTHON_MODULE OFF CACHE BOOL "create sirius python module")
set(CREATE_FORTRAN_BINDINGS ON CACHE BOOL "build Fortran bindings")
set(BUILD_DOCS OFF CACHE BOOL "build doxygen doc")
set(USE_ELPA OFF CACHE BOOL "use scalapack")
set(USE_MAGMA OFF CACHE BOOL "use MAGMA")
set(USE_CUDA OFF CACHE BOOL "use CUDA")
set(USE_MKL OFF CACHE BOOL "use Intel MKL")
set(USE_SCALAPACK OFF CACHE BOOL "use scalapack")
set(BUILD_TESTS OFF CACHE BOOL "build tests")
set(GPU_MODEL "none" CACHE STRING "The target GPU architecture; one of {none,P100,G10x0}")

set_property(CACHE GPU_MODEL PROPERTY STRINGS "none" "P100" "G10x0")

# TODO: mkl provides scalapack, make sure not both options turned on

# Set release as the default build type.
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE release CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "debug" "release" "relwithdebinfo")
endif()

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -ggdb -DDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "-ftree-vectorize -O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -ggdb -O2")
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "Intel")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2")
endif()

# include custom defined FindPackage modules
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")
include(GitSubmodule)
list(APPEND CMAKE_PREFIX_PATH $ENV{CMAKE_PREFIX_PATH})

# generate compile_commands.json with compile commands for each target
set(CMAKE_EXPORT_COMPILE_COMMANDS "YES")

if(USE_SCALAPACK)
    add_definitions("-D__SCALAPACK")
endif()

find_package(MPI REQUIRED)
find_package(GSL REQUIRED)
if(USE_MKL)
  set(USE_MKL_SHARED_LIBS On) # link against shared MKL libraries
  find_package(MKL REQUIRED)
  include_directories(BEFORE ${MKL_INCLUDE_DIR})
else()
  find_package(LAPACK REQUIRED)
  if(USE_SCALAPACK)
    find_package(ScaLAPACK REQUIRED) # just sets scalapack_DIR
    include_directories(BEFORE ${SCALAPACK_INCLUDE_DIR})
    set(SYSTEM_LIBRARIES "${SYSTEM_LIBRARIES};${SCALAPACK_LIBRARIES}")
  endif()
endif(USE_MKL)

find_package(FFTW REQUIRED)
find_package(OpenMP REQUIRED)
find_package(LibXC 3.0.0 REQUIRED)
find_package(LibSPG REQUIRED)
find_package(HDF5 REQUIRED C HL)

# cmake assumes spglib installed
#add_compile_options(-D__SPGLIB_STD_INCLUDE_PATH)

if(USE_ELPA)
  find_package(Elpa REQUIRED)
  # if(NOT USE_SCALAPACK)
  #   message(FATAL_ERROR "ELPA depends on ScaLAPACK, please set USE_SCALAPACK=On.")
  # endif()
  include_directories(BEFORE SYSTEM "${ELPA_INCLUDE_DIR}")
  set(SYSTEM_LIBRARIES "${SYSTEM_LIBRARIES};${ELPA_LIBRARIES}")
  add_definitions("-D__ELPA")
endif(USE_ELPA)

if(USE_CUDA)
  enable_language(CUDA)
  find_package(CUDA)
  add_definitions("-D__GPU")
  add_definitions("-D__CUDA")
  include_directories(BEFORE SYSTEM ${CUDA_INCLUDE_DIRS})
  set(SYSTEM_LIBRARIES "${SYSTEM_LIBRARIES};${CUDA_LIBRARIES};${CUDA_CUBLAS_LIBRARIES};${CUDA_CUFFT_LIBRARIES};${CUDA_cusolver_LIBRARY}")

  if(GPU_MODEL MATCHES "P100")
    message("set nvcc flags for P100")
    #list(APPEND CMAKE_CUDA_FLAGS "-arch=sm_60")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -arch=sm_60")
  elseif(GPU_MODEL MATCHES "G10x0")
    message("set nvcc flags for geforce 1000")
    #list(APPEND CMAKE_CUDA_FLAGS "-arch=sm_61")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -arch=sm_61")
  endif()
endif(USE_CUDA)

if(USE_MAGMA)
  if(NOT USE_CUDA)
    message(FATAL_ERROR "MAGMA depends on Cuda, must enable Cuda or disable MAGMA")
  endif(NOT USE_CUDA)
  find_package(MAGMA)
  add_definitions("-D__MAGMA")
  include_directories(BEFORE SYSTEM ${MAGMA_INCLUDE_DIR})
  set(SYSTEM_LIBRARIES "${SYSTEM_LIBRARIES};${MAGMA_LIBRARIES}")
endif(USE_MAGMA)

# add required libraries
set(SYSTEM_LIBRARIES "${SYSTEM_LIBRARIES};${MPI_CXX_LIBRARIES};${GSL_LIBRARIES};${LIBXC_LIBRARIES};${LIBSPG_LIBRARIES}")

# set OpenMP flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

# include library headers
include_directories(${FFTW_INCLUDE_DIR})
include_directories(${LIBXC_INCLUDE_DIR})
include_directories(${LIBSPG_INCLUDE_DIR})
include_directories(${HDF5_INCLUDE_DIR})
include_directories(${MPI_INCLUDE_DIR})

# project header locations
include_directories(BEFORE src)
include_directories(BEFORE src/SDDK)

# configure generation of the version header
add_custom_command(
  OUTPUT _always_rebuild
  COMMAND true
)
add_custom_command(
    OUTPUT version.hpp-test
    DEPENDS _always_rebuild
    COMMAND python ${CMAKE_SOURCE_DIR}/make_version_hpp.py ${CMAKE_SOURCE_DIR}/VERSION > version.hpp-test
)

set(version_hpp_path src/version.hpp)
add_custom_command(
    OUTPUT ${version_hpp_path}
    DEPENDS version.hpp-test
    COMMAND ${CMAKE_COMMAND} -E copy_if_different version.hpp-test ${version_hpp_path}
)

add_custom_target(generate_version_hpp DEPENDS ${version_hpp_path})


## configure version header
#exec_program("git" ${CMAKE_CURRENT_SOURCE_DIR}
#  ARGS "rev-parse HEAD"
#  OUTPUT_VARIABLE GIT_VERSION_SHA1
#  RETURN_VALUE exit_code)
#if(exit_code)
#  set(GIT_VERSION_SHA1 "")
#endif()
#exec_program("date"
#  ARGS "+\"%a, %e %b %Y %H:%M:%S\""
#  OUTPUT_VARIABLE BUILD_DATE)
#exec_program(
#  "git"
#  ${CMAKE_CURRENT_SOURCE_DIR}
#  ARGS "describe --all"
#  OUTPUT_VARIABLE GIT_BRANCHNAME
#  RETURN_VALUE exit_code)
#if(exit_code)
#  set(GIT_BRANCHNAME "")
#endif()
#configure_file("${PROJECT_SOURCE_DIR}/src/version.hpp.in" "${PROJECT_BINARY_DIR}/src/version.hpp")
include_directories(BEFORE ${PROJECT_BINARY_DIR}/src)

MACRO(SIRIUS_SETUP_TARGET _target)
  if(USE_CUDA)
    add_dependencies(${_target} sirius_cu)
  endif()

  add_dependencies(${_target} generate_version_hpp)

  if(USE_MKL)
    # TODO: handle -lpthread properly
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      target_link_libraries(${_target} PRIVATE "${sirius_cu_location};${SYSTEM_LIBRARIES};${HDF5_C_LIBRARIES};${HDF5_C_HL_LIBRARIES};-Wl,--no-as-needed;${MKL_LIBRARIES};-lpthread -lm -ldl -lgomp")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
      target_link_libraries(${_target} PRIVATE "${sirius_cu_location};${SYSTEM_LIBRARIES};${MKL_LIBRARIES};${HDF5_C_LIBRARIES};${HDF5_C_HL_LIBRARIES};-lpthread")
    else()
      message(FATAL_ERROR "Unsupported compiler")
    endif()
  else()
    target_link_libraries(${_target} PRIVATE "${sirius_cu_location};${SYSTEM_LIBRARIES};${LAPACK_LIBRARIES};${FFTW_LIBRARIES};${HDF5_C_LIBRARIES};${HDF5_C_HL_LIBRARIES}")
  endif()
ENDMACRO()

# sirius library
add_subdirectory(src)
if(USE_CUDA)
  set(sirius_cu_location $<TARGET_FILE:sirius_cu>)
endif()

# applications
add_subdirectory(apps/atoms)
add_subdirectory(apps/dft_loop)
if(BUILD_TESTS)
  add_subdirectory(apps/tests)
  add_subdirectory(apps/unit_tests)
endif(BUILD_TESTS)
add_subdirectory(apps/utils)
add_subdirectory(python_module)
add_subdirectory(doc)
