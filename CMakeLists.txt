CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

IF(AMIGA)
    set(SOURCES
        AmigaDOS/main.cpp
        AmigaDOS/OSSymbols.cpp
        AmigaDOS/Process.cpp
        AmigaDOS/Breaks.cpp
        AmigaDOS/LowLevel.cpp
        #AmigaDOS/Definitions.cpp
        #AmigaDOS/Interpreter.cpp
        AmigaDOS/ElfHandle.cpp
        AmigaDOS/Interpreter02.cpp
        AmigaDOS/StringTools.cpp
    )

    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -athread=native")

    ADD_EXECUTABLE(Iteration01 AmigaDOS/ProcessHandler01.cpp)
    ADD_EXECUTABLE(Iteration02 AmigaDOS/ProcessHandler02.cpp)
    ADD_EXECUTABLE(Iteration03 AmigaDOS/ProcessHandler03.cpp)
    ADD_EXECUTABLE(Iteration04 AmigaDOS/ProcessHandler04.cpp)
    ADD_EXECUTABLE(Iteration05 AmigaDOS/ProcessHandler05.cpp)
    target_link_libraries(Iteration05 -lauto)
    ADD_EXECUTABLE(Iteration06 AmigaDOS/ProcessHandler06.cpp)
    target_link_libraries(Iteration06 -lauto)

    add_executable(SimpleDebug ${SOURCES})
    target_link_libraries(SimpleDebug -lauto)
ENDIF()
