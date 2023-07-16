# -------------------------------------------------------------------------
# FIND std::execution

find_package(Threads REQUIRED)
find_package(TBB QUIET)

if(NOT DEFINED STD_EXECUTION_LIBRARIES)
    set(STD_EXECUTION_LIBRARIES Threads::Threads CACHE INTERNAL "Libraries for std::execution")
endif()

include(CheckCXXSourceRuns)
set(execution_test_src "
#include <algorithm>
#include <execution>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
int main() {
    std::mutex mtx;
    std::set<std::thread::id> threads;
    std::vector<int> range(100);
    std::for_each(std::execution::par, begin(range), end(range),
        [&](auto i) {
            {
                std::unique_lock l{mtx};
                threads.insert(std::this_thread::get_id());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    return threads.size() > 1 ? 0 : 1;
}
")

if (NOT DEFINED HAVE_STD_EXECUTION)
    message(STATUS "Checking for C++17 execution policies")
endif()
set(CMAKE_REQUIRED_LIBRARIES ${STD_EXECUTION_LIBRARIES})
check_cxx_source_runs("${execution_test_src}" HAVE_STD_EXECUTION)

if(NOT HAVE_STD_EXECUTION AND TBB_FOUND)
    set(STD_EXECUTION_LIBRARIES Threads::Threads TBB::tbb CACHE INTERNAL "Libraries for std::execution")
    unset(HAVE_STD_EXECUTION CACHE)
    message(STATUS "Checking for C++17 execution policies with -ltbb")
    set(CMAKE_REQUIRED_LIBRARIES ${STD_EXECUTION_LIBRARIES})
    check_cxx_source_runs("${execution_test_src}" HAVE_STD_EXECUTION)
endif()

if(NOT HAVE_STD_EXECUTION)
    if(NOT TBB_FOUND AND ${CMAKE_SYSTEM_NAME} STREQUAL Linux)
        message(WARNING "Try installing TBB to enable C++17 execution policies")
    endif()
    message(FATAL_ERROR "C++17 execution policies not working")
endif()

# -------------------------------------------------------------------------
# FIND GDBM

find_library(GDBM_LIBRARY NAMES gdbm)
if (GDBM_LIBRARY STREQUAL GDBM_LIBRARY-NOTFOUND)
    message(FATAL_ERROR "libgdbm not found!")
endif()

include(CheckCXXSymbolExists)
set(CMAKE_REQUIRED_LIBRARIES ${GDBM_LIBRARY})
check_cxx_symbol_exists(gdbm_open "gdbm.h" HAVE_GDBM_H)

if(NOT HAVE_GDBM_H)
    message(FATAL_ERROR "no gdbm.h")
endif()

# -------------------------------------------------------------------------
# DEFINE TARGETS

add_executable(polycubegen polycubegen.cpp)
target_link_libraries(polycubegen ${STD_EXECUTION_LIBRARIES} ${GDBM_LIBRARY})

add_executable(polycubes2obj polycubes2obj.cpp)

install(TARGETS polycubegen polycubes2obj)
