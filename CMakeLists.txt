cmake_minimum_required(VERSION 4.3)

project (FileEventSystem CXX C)
include(FetchContent)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(FileEventSystem src/main.cpp)
add_executable(Inotify src/inotify_test.cpp)
FetchContent_Declare(
  magic_enum
  GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
  GIT_TAG        v0.9.5 # Use the latest version
)
FetchContent_MakeAvailable(magic_enum)
set("MAGIC_ENUM_LIBRARIES" magic_enum::magic_enum)
target_include_directories(FileEventSystem PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/includes)
target_link_libraries(FileEventSystem PRIVATE 
   ${MAGIC_ENUM_LIBRARIES})

target_include_directories(Inotify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/includes)
target_link_libraries(Inotify PRIVATE 
   ${MAGIC_ENUM_LIBRARIES})