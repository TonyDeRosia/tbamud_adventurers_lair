cmake_minimum_required(VERSION 3.27)

# Set the project name
project(circle)

# Global definitions
if(MSVC)
	# using Visual Studio C++
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
endif()

# circle itself
file(GLOB CIRCLE_SOURCES
        "*.h"
        "*.c"
)

add_executable(circle ${CIRCLE_SOURCES})

find_library(CRYPT_LIBRARY crypt)
find_library(CRYPTO_LIBRARY crypto)

if(CRYPT_LIBRARY)
        target_link_libraries(circle ${CRYPT_LIBRARY})
endif()

if(CRYPTO_LIBRARY)
        target_link_libraries(circle ${CRYPTO_LIBRARY})
endif()

if(NOT MSVC)
        target_link_libraries(circle m)
endif()
 
if(MSVC)
        target_link_libraries(circle wsock32.lib)

        set_target_properties(circle PROPERTIES
                VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
        )
endif()
