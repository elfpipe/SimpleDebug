CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

IF(AMIGA)
    set(SOURCES
        AmigaDOS/main.cpp
        AmigaDOS/OSSymbols.cpp
        AmigaDOS/Process.cpp
        AmigaDOS/Breaks.cpp
        AmigaDOS/LowLevel.cpp
        AmigaDOS/ElfHandle.cpp
        AmigaDOS/Interpreter02.cpp
        AmigaDOS/StringTools.cpp
        AmigaDOS/Tracer.cpp
    )

    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -athread=native")

    add_executable(SimpleDebug ${SOURCES})
    target_link_libraries(SimpleDebug -lauto)
ENDIF()
