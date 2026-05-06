set(TEST_EXE0 "cli_parser_test01.exe")

add_executable(${TEST_EXE0} src/test01.cpp)
target_link_libraries(${TEST_EXE0} PRIVATE CLI::PARSER)
