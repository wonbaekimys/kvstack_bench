cmake_minimum_required(VERSION 3.24.2)

project(KVStackBenchmark C CXX)

if (NOT DEFINED CMAKE_CXX_STANDARD)
  set (CMAKE_CXX_STANDARD 17)
endif()

set(CMAKE_BUILD_TYPE "Release")

find_package(Threads REQUIRED)
#find_package(TBB REQUIRED)
find_package(LibCDS REQUIRED)

if(GRPC_AS_SUBMODULE)
  # One way to build a projects that uses gRPC is to just include the
  # entire gRPC project tree via "add_subdirectory".
  # This approach is very simple to use, but the are some potential
  # disadvantages:
  # * it includes gRPC's CMakeLists.txt directly into your build script
  #   without and that can make gRPC's internal setting interfere with your
  #   own build.
  # * depending on what's installed on your system, the contents of submodules
  #   in gRPC's third_party/* might need to be available (and there might be
  #   additional prerequisites required to build them). Consider using
  #   the gRPC_*_PROVIDER options to fine-tune the expected behavior.
  #
  # A more robust approach to add dependency on gRPC is using
  # cmake's ExternalProject_Add (see cmake_externalproject/CMakeLists.txt).

  # Include the gRPC's cmake build (normally grpc source code would live
  # in a git submodule called "third_party/grpc", but this example lives in
  # the same repository as gRPC sources, so we just look a few directories up)
  add_subdirectory(../../.. ${CMAKE_CURRENT_BINARY_DIR}/grpc EXCLUDE_FROM_ALL)
  message(STATUS "Using gRPC via add_subdirectory.")

  # After using add_subdirectory, we can now use the grpc targets directly from
  # this build.
  set(_PROTOBUF_LIBPROTOBUF libprotobuf)
  set(_REFLECTION grpc++_reflection)
  if(CMAKE_CROSSCOMPILING)
    find_program(_PROTOBUF_PROTOC protoc)
  else()
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
  endif()
  set(_GRPC_GRPCPP grpc++)
  if(CMAKE_CROSSCOMPILING)
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
  else()
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
  endif()
elseif(GRPC_FETCHCONTENT)
  # Another way is to use CMake's FetchContent module to clone gRPC at
  # configure time. This makes gRPC's source code available to your project,
  # similar to a git submodule.
  message(STATUS "Using gRPC via add_subdirectory (FetchContent).")
  include(FetchContent)
  FetchContent_Declare(
    grpc
    GIT_REPOSITORY https://github.com/grpc/grpc.git
    # when using gRPC, you will actually set this to an existing tag, such as
    # v1.25.0, v1.26.0 etc..
    # For the purpose of testing, we override the tag used to the commit
    # that's currently under test.
    GIT_TAG        vGRPC_TAG_VERSION_OF_YOUR_CHOICE)
  FetchContent_MakeAvailable(grpc)

  # Since FetchContent uses add_subdirectory under the hood, we can use
  # the grpc targets directly from this build.
  set(_PROTOBUF_LIBPROTOBUF libprotobuf)
  set(_REFLECTION grpc++_reflection)
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
  set(_GRPC_GRPCPP grpc++)
  if(CMAKE_CROSSCOMPILING)
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
  else()
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
  endif()
else()
  # This branch assumes that gRPC and all its dependencies are already installed
  # on this system, so they can be located by find_package().

  # Find Protobuf installation
  # Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
  set(protobuf_MODULE_COMPATIBLE TRUE)
  find_package(Protobuf CONFIG REQUIRED)
  message(STATUS "Using protobuf ${Protobuf_VERSION}")

  set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
  set(_REFLECTION gRPC::grpc++_reflection)
  if(CMAKE_CROSSCOMPILING)
    find_program(_PROTOBUF_PROTOC protoc)
  else()
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
  endif()

  # Find gRPC installation
  # Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
  find_package(gRPC CONFIG REQUIRED)
  message(STATUS "Using gRPC ${gRPC_VERSION}")

  set(_GRPC_GRPCPP gRPC::grpc++)
  if(CMAKE_CROSSCOMPILING)
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
  else()
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
  endif()
endif()

# Proto files
get_filename_component(simplekv_proto "protos/simplekv.proto" ABSOLUTE)
get_filename_component(simplekv_proto_path "${simplekv_proto}" PATH)

# Generated sources
set(simplekv_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/simplekv.pb.cc")
set(simplekv_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/simplekv.pb.h")
set(simplekv_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/simplekv.grpc.pb.cc")
set(simplekv_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/simplekv.grpc.pb.h")
set(simplekv_proto_py "${CMAKE_CURRENT_BINARY_DIR}/simplekv_pb2.py")
set(simplekv_grpc_py "${CMAKE_CURRENT_BINARY_DIR}/simplekv_pb2_grpc.py")
add_custom_command(
    #OUTPUT "${simplekv_proto_srcs}" "${simplekv_proto_hdrs}" "${simplekv_grpc_srcs}" "${simplekv_grpc_hdrs}"
    OUTPUT "${simplekv_proto_srcs}" "${simplekv_proto_hdrs}" "${simplekv_grpc_srcs}" "${simplekv_grpc_hdrs}" "${simplekv_proto_py}" "${simplekv_grpc_py}"
    COMMAND "${_PROTOBUF_PROTOC}"
    ARGS  --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
          --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
          -I "${simplekv_proto_path}"
          --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
          "${simplekv_proto}"
    COMMAND "python3"
    ARGS  -m grpc_tools.protoc
          --python_out "${CMAKE_CURRENT_BINARY_DIR}"
          --grpc_python_out "${CMAKE_CURRENT_BINARY_DIR}"
          -I "${simplekv_proto_path}"
          "${simplekv_proto}"
    DEPENDS "${simplekv_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# simplekv_grpc_proto
add_library(simplekv_grpc_proto
    ${simplekv_grpc_srcs}
    ${simplekv_grpc_hdrs}
    ${simplekv_proto_srcs}
    ${simplekv_proto_hdrs})
target_link_libraries(simplekv_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

# client
get_filename_component(simplekv_client_py_src "simplekv_client.py" ABSOLUTE)
set(simplekv_client_py "${CMAKE_CURRENT_BINARY_DIR}/simplekv_client.py")
configure_file("${simplekv_client_py_src}" "${simplekv_client_py}" COPYONLY)

# server
add_executable(simplekv_server "simplekv_server.cc")
target_link_libraries(simplekv_server
    simplekv_grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ~/.local/lib/libcds.so)

# benchmark
get_filename_component(kvstack_bench_py_src "kvstack_bench.py" ABSOLUTE)
get_filename_component(kvstack_init_lock_py_src "kvstack_init_lock.py" ABSOLUTE)
get_filename_component(kvstack_load_py_src "kvstack_load.py" ABSOLUTE)
get_filename_component(benchmark_run_script_src "scripts/run_benchmark.sh" ABSOLUTE)
set(kvstack_bench_py "${CMAKE_CURRENT_BINARY_DIR}/kvstack_bench.py")
set(kvstack_init_lock_py "${CMAKE_CURRENT_BINARY_DIR}/kvstack_init_lock.py")
set(kvstack_load_py "${CMAKE_CURRENT_BINARY_DIR}/kvstack_load.py")
set(benchmark_run_script "${CMAKE_CURRENT_BINARY_DIR}/run_benchmark.sh")
configure_file("${kvstack_bench_py_src}" "${kvstack_bench_py}" COPYONLY)
configure_file("${kvstack_init_lock_py_src}" "${kvstack_init_lock_py}" COPYONLY)
configure_file("${kvstack_load_py_src}" "${kvstack_load_py}" COPYONLY)
configure_file("${benchmark_run_script_src}" "${benchmark_run_script}" COPYONLY)

# kvstack local benchmark
add_executable(kvstack_local_test "kvstack_local_test.cc")
target_link_libraries(kvstack_local_test
    pthread
    ~/.local/lib/libcds.so)
add_executable(kvstack_mutex_local_test "kvstack_mutex_local_test.cc")
target_link_libraries(kvstack_mutex_local_test
    pthread)

# simplekv local benchmark
add_executable(simplekv_test "simplekv_test.cc")
target_link_libraries(simplekv_test
    pthread
    ~/.local/lib/libcds.so)
add_executable(simplekv_mutex_test "simplekv_mutex_test.cc")
target_link_libraries(simplekv_mutex_test
    pthread)
