cmake_minimum_required(VERSION 3.10)
project(image_snapshotter)

find_package(ament_cmake REQUIRED)
find_package(image_snapshotter_interfaces REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(rmw REQUIRED)
find_package(sensor_msgs REQUIRED)

add_library(${PROJECT_NAME} SHARED
  src/image_snapshotter.cpp
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    rclcpp::rclcpp
    rclcpp_components::component
    rmw::rmw
    image_snapshotter_interfaces::image_snapshotter_interfaces__rosidl_typesupport_cpp
    sensor_msgs::sensor_msgs__rosidl_typesupport_cpp
)

rclcpp_components_register_node(
  ${PROJECT_NAME}
  PLUGIN "image_snapshotter::ImageSnapshotter"
  EXECUTABLE image_snapshotter_node
)

install(
  TARGETS ${PROJECT_NAME}
  LIBRARY DESTINATION lib
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
