cmake_minimum_required (VERSION 3.2)
project (lazy-stingization)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(
    Boost
    COMPONENTS
		program_options
		log
        system
    REQUIRED
)

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})

    MESSAGE(STATUS "boost inc: " ${Boost_INCLUDE_DIRS})
    MESSAGE(STATUS "boost libs: " ${Boost_LIBRARIES})
endif()

# prevent warnings about wcscpy & friends
add_definitions(-D_CRT_SECURE_NO_WARNINGS)
add_definitions(-DBOOST_LOG_DYN_LINK)
add_definitions(-DBOOST_ALL_DYN_LINK)
add_definitions(-D_WIN32_WINNT=0x0601)

add_executable(lazy-stingization
        src/main.cpp
)
target_link_libraries(lazy-stingization ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

#enable_testing()
#add_subdirectory("tests")
