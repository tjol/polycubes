find_package(TBB REQUIRED)

add_executable(polycubegen polycubegen.cpp)
target_link_libraries(polycubegen TBB::tbb)
