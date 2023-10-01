cmake_minimum_required(VERSION 3.5)
project(giskard_core)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)

endif()

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)

## Finding system dependencies which come without cmake
find_package(PkgConfig)
pkg_check_modules(YamlCpp yaml-cpp)
find_path(yaml_cpp_INCLUDE_DIRS yaml-cpp/yaml.h PATH_SUFFIXES include)
find_library(yaml_cpp_LIBRARIES NAMES yaml-cpp)

find_package(Eigen3 REQUIRED)
find_package(urdf REQUIRED)
find_package(expressiongraph REQUIRED)
find_package(qpOASES REQUIRED)
find_package(kdl_parser REQUIRED)

#find_library(KDL_LIBRARY REQUIRED NAMES orocos-kdl HINTS ${orocos_kdl_LIBRARY_DIRS})

##############
## Building ##
##############

# setting additional cmake flags, used for integration with Coveralls.io
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized -Winit-self -Wunused-function -Wunused-label -Wunused-variable -Wunused-but-set-parameter")#-Wunused-but-set-variable 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wreorder -Warray-bounds -Wtype-limits -Werror=return-type -Wsequence-point -Wparentheses -Wmissing-braces -Wchar-subscripts -Wswitch -Wwrite-strings -Wenum-compare -Wempty-body")# -Wlogical-op")

# Add headers here also for the test below
include_directories(include ${urdf_INCLUDE_DIRS} ${kdl_parser_INCLUDE_DIRS} ${qpOASES_INCLUDE_DIRS} ${expressiongraph_INCLUDE_DIRS})

add_executable(extract_expression src/${PROJECT_NAME}/extract_expression.cpp)
ament_target_dependencies(extract_expression expressiongraph qpOASES urdf kdl_parser)
target_include_directories(extract_expression PUBLIC ${EIGEN3_INCLUDE_DIR} ${urdf_INCLUDE_DIRS} ${kdl_parser_INCLUDE_DIRS}
                           ${qpOASES_INCLUDE_DIRS} ${expressiongraph_INCLUDE_DIRS})
target_link_libraries(extract_expression ${yaml_cpp_LIBRARIES} ${urdf_LIBRARIES} ${kdl_parser_LIBRARIES}
                      ${qpOASES_LIBRARIES}  ${expressiongraph_LIBRARIES})

#############
## Testing ##
#############

set(TEST_SRCS
  test/main.cpp
  test/${PROJECT_NAME}/boxy_fk.cpp
  test/${PROJECT_NAME}/double_expression_generation.cpp
  test/${PROJECT_NAME}/expression_arrays.cpp
  test/${PROJECT_NAME}/equality.cpp
  test/${PROJECT_NAME}/frame_expression_generation.cpp
  test/${PROJECT_NAME}/flying_cup.cpp
  test/${PROJECT_NAME}/pr2_cart_cart.cpp
  test/${PROJECT_NAME}/pr2_fk.cpp
  test/${PROJECT_NAME}/pr2_ik.cpp
  test/${PROJECT_NAME}/qp_controller.cpp
  test/${PROJECT_NAME}/qp_problem_builder.cpp
  test/${PROJECT_NAME}/rotation_control.cpp
  test/${PROJECT_NAME}/rotation_expression_generation.cpp
  test/${PROJECT_NAME}/robot.cpp
  test/${PROJECT_NAME}/scope.cpp
  test/${PROJECT_NAME}/slerp.cpp
  test/${PROJECT_NAME}/vector_expression_generation.cpp
  test/${PROJECT_NAME}/qp_controller_projection.cpp
  test/${PROJECT_NAME}/qp_controller_spec_generator.cpp
  test/${PROJECT_NAME}/yaml_parser.cpp
  )

if (BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(${PROJECT_NAME}-test ${TEST_SRCS}
                  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/test_data)
  ament_target_dependencies(${PROJECT_NAME}-test expressiongraph qpOASES urdf kdl_parser)
  target_link_libraries(${PROJECT_NAME}-test ${yaml_cpp_LIBRARIES} ${urdf_LIBRARIES} ${kdl_parser_LIBRARIES}
                        ${qpOASES_LIBRARIES}  ${expressiongraph_LIBRARIES})
endif()

install(
  DIRECTORY include/${PROJECT_NAME}/
  DESTINATION include/${PROJECT_NAME}
)

# Export dependencies, include for downstream/client CMake-based packages
ament_export_dependencies(urdf kdl_parser yaml_cpp expressiongraph qpOASES)
ament_export_include_directories(include)
#ament_export_libraries(${PROJECT_NAME})
ament_package()