set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror \
    -Wno-unused-result -Wno-unused-parameter -Wno-unused-function \
    -Wno-missing-field-initializers -Wno-missing-braces")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
set(CMAKE_C_STANDARD 11)

set(HEADERS
        )

set(SOURCES
        src/dmgdb.c
        )

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

add_executable(dmgdb ${HEADERS} ${SOURCES})
target_link_libraries(dmgdb libdmg pthread ${SDL2_LIBRARY})
