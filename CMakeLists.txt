CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

IF(AMIGA)
    set(SOURCES
        AmigaDOS/main.cpp
        AmigaDOS/Symbols.cpp
        AmigaDOS/Process.cpp
        AmigaDOS/Breaks.cpp
        AmigaDOS/LowLevel.cpp
        AmigaDOS/Handle.cpp
        AmigaDOS/Binary.cpp
        AmigaDOS/Strings.cpp
        AmigaDOS/Tracer.cpp
        AmigaDOS/Pipe.cpp
        AmigaDOS/async.c
    )

    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -athread=native")

    add_executable(SimpleDebug ${SOURCES})
    target_link_libraries(SimpleDebug -lauto)

    add_executable(PipeTest AmigaDOS/Test/pipetest.cpp AmigaDOS/Pipe.cpp)
    target_link_libraries(SimpleDebug -lauto)
ENDIF()
