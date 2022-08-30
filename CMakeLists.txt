cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
project(gpu-let)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_TYPE Debug)
set(GPULET_ROOT $ENV{PWD})

find_package(Torch REQUIRED)
message(STATUS "Find Torch VERSION: ${Torch_VERSION}")
add_definitions(-DTORCH_VERSION_MAJOR=${Torch_VERSION_MAJOR})
add_definitions(-DTORCH_VERSION_MINOR=${Torch_VERSION_MINOR})
add_definitions(-DTORCH_VERSION_PATCH=${Torch_VERSION_PATCH})
find_package(OpenCV REQUIRED)

message(${OpenCV_INCLUDE_DIRS})

include_directories(
	#include
    src/common
	src/proxy
	src/scheduler
	src/service
	#${TONIC_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

file(GLOB COMMON_SRCS src/common/*.cpp)
file(GLOB SERVER_SRCS src/service/*.cpp)
file(GLOB SCHEDULER_SRCS src/scheduler/*.cpp)


#------Essential Flags for experiment-----
add_compile_definitions(EPOCH_DEBUG)
add_compile_definitions(CHECK_MEM)
add_compile_definitions(CHECK_NETBW)
add_compile_definitions(PROFILE)
add_compile_definitions(SCALE)
add_compile_definitions(ADD_RATE)
add_compile_definitions(PROXY_LOG)

#-----Useful Flags for Debugging----------
add_compile_definitions(NO_NET)
add_compile_definitions(BACKEND_DEBUG)
add_compile_definitions(FRONTEND_DEBUG)
add_compile_definitions(DEBUG)
add_compile_definitions(SCHED_DEBUG)
add_compile_definitions(SCALE_DEBUG)
add_compile_definitions(DROP_DEBUG)
#add_compile_definitions(ST_DEBUG)
#add_compile_definitions(LB_DEBUG)
#add_compile_definitions(OVERHEAD)

#----- Flags left in case someone finds them useful -------
#add_compile_definitions(EAGER_DROP)
#add_compile_definitions(DEV_LOG)
#add_compile_definitions(MUTRACE)


#---------standalone_inference---------

add_executable(standalone_inference src/standalone_inference.cpp ${SCHEDULER_SRCS} ${COMMON_SRCS})
target_link_libraries(standalone_inference "${TORCH_LIBRARIES}" ${OpenCV_LIBS} boost_program_options  boost_system nvidia-ml)
set_property(TARGET standalone_inference PROPERTY CXX_STANDARD 14)     

#---------client---------
add_executable(client src/client.cpp
        src/common/socket.cpp
        src/common/img_utils.cpp
        src/common/torch_utils.cpp
        src/common/common_utils.cpp
        src/common/exp_model.cpp
        )
target_link_libraries(client "${TORCH_LIBRARIES}" ${OpenCV_LIBS} boost_program_options  boost_system nvidia-ml)
set_property(TARGET client PROPERTY CXX_STANDARD 14)
