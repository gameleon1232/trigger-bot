cmake_minimum_required(VERSION 3.16)
project(TrigerBotCPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Console version (replicates auto_clicker.py)
add_executable(AutoClickerConsole src/AutoClickerConsole.cpp)
target_link_libraries(AutoClickerConsole PRIVATE user32 gdi32 winmm)

# GUI version (replicates auto_clicker_gui.py)
add_executable(AutoClickerGUI WIN32 src/AutoClickerGUI.cpp)
target_link_libraries(AutoClickerGUI PRIVATE user32 gdi32 comctl32 winmm)

