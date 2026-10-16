##############################################################################
# CMake
##############################################################################
set (CMAKE_CXX_STANDARD 11)
cmake_minimum_required(VERSION 2.8.0)
project(server_node)
set(CMAKE_INCLUDE_CURRENT_DIR ON)#added

##############################################################################
# Catkin
##############################################################################

# qt_build provides the qt cmake glue, roscpp the comms for a default talker
find_package(catkin REQUIRED COMPONENTS qt_build roscpp rviz cv_bridge velodyne_msgs image_transport sensor_msgs geometry_msgs)
find_package(Qt5 REQUIRED Core Gui Widgets Network)#added
set(QT_LIBRARIES Qt5::Widgets Qt5::Network Qt5::Core Qt5::Gui)#added
include_directories(${catkin_INCLUDE_DIRS}
                    ${OpenCV_INCLUDE_DIRS})

# Use this to define what the package will export (e.g. libs, headers).
# Since the default here is to produce only a binary, we don't worry about
# exporting anything.
catkin_package()

##############################################################################
# OpenCV Environment
##############################################################################

find_package(OpenCV 3 REQUIRED)
message(STATUS "opencv version:" ${OpenCV_VERSION})
message(STATUS "opencv lib:" ${OpenCV_LIBS})
message(STATUS "opencv include dir:" ${OpenCV_INCLUDE_DIRS})
message(STATUS "opencv config path:" ${OpenCV_CONFIG_PATH})

# this comes from qt_build's qt-ros.cmake which is automatically
# included via the dependency call in package.xml
#rosbuild_prepare_qt4(QtCore QtGui) # Add the appropriate components to the component list here

##############################################################################
# Sections
##############################################################################

file(GLOB QT_FORMS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ui/*.ui)
file(GLOB QT_RESOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} resources/*.qrc)
file(GLOB_RECURSE QT_MOC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} FOLLOW_SYMLINKS include/server_node/*.hpp *.h)

QT5_ADD_RESOURCES(QT_RESOURCES_CPP ${QT_RESOURCES})
QT5_WRAP_UI(QT_FORMS_HPP ${QT_FORMS})
QT5_WRAP_CPP(QT_MOC_HPP ${QT_MOC})

##############################################################################
# Sources
##############################################################################

file(GLOB_RECURSE QT_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} FOLLOW_SYMLINKS src/*.cpp)

##############################################################################
# Binaries
##############################################################################

add_executable(server_node ${QT_SOURCES} ${QT_RESOURCES_CPP} ${QT_FORMS_HPP} ${QT_MOC_HPP})
target_link_libraries(server_node ${QT_LIBRARIES}
                                ${catkin_LIBRARIES}
                                ${OpenCV_LIBRARIES})
install(TARGETS server_node RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})
