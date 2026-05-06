/**
 * @file test01.cpp
 * @author Robert Morley
 * @date 2026-05-06
 *
 * @brief
 *
 * @version v0.1.0
 * @copyright (c) Copyright 2026
 */
#include <cstdio>
#include "cli_parser.hpp"

// Command handler function
void handle_echo(const cli::ParseResult<>& result) {
  // Access arguments
  for (const auto& arg : result.arguments()) {
    printf("%s ", arg.data());
  }
  printf("\n");
}

// Create command registry
constexpr auto my_registry = cli::registry()
    .add_command(
        cli::command("echo")
            .description("Echo arguments back")
            .args(1, 8)  // min 1, max 8 arguments
            .handler(handle_echo)
    )
    .build();

// Parse and execute
int main() {
  const auto input = "echo hello world";

  if (const auto result = cli::parse_and_execute<32, 16, 8, 32>(input, my_registry); !result.is_valid()) {
    printf("Error: %s\n", cli::error_string(result.error).data());
  }

  return 0;
}