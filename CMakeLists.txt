cmake_minimum_required(VERSION 4.0.0)

project("Command Processor"
        LANGUAGES CXX C ASM
        VERSION "0.1.0"
        DESCRIPTION "Simple command parser for microcontrollers."
)

add_library("cli_parser" INTERFACE)
target_sources("cli_parser" INTERFACE
        FILE_SET cli_header TYPE HEADERS
        BASE_DIRS src
        FILES src/cli_parser.hpp
)
add_library(CLI::PARSER ALIAS "cli_parser")
add_subdirectory(tests)
