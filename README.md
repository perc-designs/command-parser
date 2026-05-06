# Command Parser Library

A zero-overhead, compile-time configured command line parser for embedded systems.

## Features

- **Zero Dynamic Allocation**: All memory is statically allocated using fixed-size arrays
- **Compile-Time Configuration**: Commands and options are registered at compile time
- **Type Safety**: Leverages C++23 concepts for type-safe interfaces
- **Flexible Options**: Support for both long (`--option`) and short (`-o`) flags
- **Boolean Flags**: Options can be boolean (no value) or value-based
- **Argument Validation**: Specify minimum and maximum argument counts
- **Integrated Handlers**: Each command can have an associated handler function
- **Zero-Copy**: Uses `std::string_view` for efficient string handling
- **Small Footprint**: ~2-5 KB flash, ~512 bytes RAM during parsing

## Memory Usage

- **Flash**: ~2-5 KB (depending on number of commands)
- **RAM**: ~512 bytes during parsing (customizable via template parameters)

## Quick Start

### 1. Basic Command with No Options

This example shows a simple command that takes no options, only arguments.

```cpp
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
    const char* input = "echo hello world";
    auto result = cli::parse_and_execute<32, 16, 8, 32>(input, my_registry);

    if (!result.is_valid()) {
        printf("Error: %s\n", cli::error_string(result.error).data());
    }

    return 0;
}
```

**Usage:**
```
echo hello world
```

**Output:**
```
hello world
```

### 2. Command with Multiple Options

This example demonstrates a command with both boolean flags and value-based options.

```cpp
#include "cli_parser.hpp"
#include <cstdio>
#include <cstdlib>

// Command handler for LED control
void handle_led(const cli::ParseResult<>& result) {
    // Check for boolean flag
    bool verbose = result.has_option("verbose");

    // Get option values with defaults
    auto color = result.get_option_value("color", "white");
    auto brightness = result.get_option_value("brightness", "100");

    // Access positional arguments
    if (result.arg_count > 0) {
        auto led_id = result.args[0];

        if (verbose) {
            printf("Setting LED %.*s:\n",
                   static_cast<int>(led_id.size()), led_id.data());
            printf("  Color: %.*s\n",
                   static_cast<int>(color.size()), color.data());
            printf("  Brightness: %.*s\n",
                   static_cast<int>(brightness.size()), brightness.data());
        }

        // Convert brightness to integer
        int brightness_val = std::atoi(brightness.data());

        // Your LED control logic here
        // set_led(led_id, color, brightness_val);
    }
}

// Create command registry with options
constexpr auto led_registry = cli::registry()
    .add_command(
        cli::command("led")
            .description("Control LED settings")
            .exact_args(1)  // Exactly 1 argument (LED ID)
            .add_option(
                cli::option("color")
                    .short_flag('c')
                    .with_value()
                    .description("LED color (red, green, blue, white)")
            )
            .add_option(
                cli::option("brightness")
                    .short_flag('b')
                    .with_value()
                    .description("Brightness level (0-100)")
            )
            .add_option(
                cli::option("verbose")
                    .short_flag('v')
                    .boolean()
                    .description("Enable verbose output")
            )
            .handler(handle_led)
    )
    .build();

int main() {
    // Example 1: Using long option names
    const char* cmd1 = "led --color red --brightness 75 --verbose 1";
    auto result1 = cli::parse_and_execute<32, 16, 8, 32>(cmd1, led_registry);

    // Example 2: Using short option names
    const char* cmd2 = "led -c blue -b 50 2";
    auto result2 = cli::parse_and_execute<32, 16, 8, 32>(cmd2, led_registry);

    // Example 3: Using defaults (no options)
    const char* cmd3 = "led 3";
    auto result3 = cli::parse_and_execute<32, 16, 8, 32>(cmd3, led_registry);

    return 0;
}
```

**Usage Examples:**
```
led --color red --brightness 75 --verbose 1
led -c blue -b 50 2
led 3
```

**Output:**
```
Setting LED 1:
  Color: red
  Brightness: 75
```

### 3. Multiple Commands in One Registry

```cpp
#include "cli_parser.hpp"

void handle_status(const cli::ParseResult<>& result) {
    printf("System Status: OK\n");
    printf("Uptime: 12345 seconds\n");
}

void handle_reset(const cli::ParseResult<>& result) {
    bool force = result.has_option("force");

    if (force) {
        printf("Force reset initiated...\n");
    } else {
        printf("Normal reset initiated...\n");
    }
}

void handle_config(const cli::ParseResult<>& result) {
    auto key = result.args[0];
    auto value = result.args[1];

    printf("Setting %.*s = %.*s\n",
           static_cast<int>(key.size()), key.data(),
           static_cast<int>(value.size()), value.data());
}

// Create a registry with multiple commands
constexpr auto system_registry = cli::registry()
    .add_command(
        cli::command("status")
            .description("Show system status")
            .exact_args(0)
            .handler(handle_status)
    )
    .add_command(
        cli::command("reset")
            .description("Reset the system")
            .exact_args(0)
            .add_option(
                cli::option("force")
                    .short_flag('f')
                    .boolean()
                    .description("Force reset without confirmation")
            )
            .handler(handle_reset)
    )
    .add_command(
        cli::command("config")
            .description("Configure system settings")
            .exact_args(2)  // key and value
            .handler(handle_config)
    )
    .build();

int main() {
    const char* commands[] = {
        "status",
        "reset --force",
        "config wifi_ssid MyNetwork"
    };

    for (const auto& cmd : commands) {
        auto result = cli::parse_and_execute<32, 16, 8, 32>(cmd, system_registry);

        if (!result.is_valid()) {
            printf("Error parsing '%s': %s\n",
                   cmd, cli::error_string(result.error).data());
        }
    }

    return 0;
}
```

## Implementing Command Handlers

### Handler Function Signature

Command handlers must match this signature:

```cpp
void handler_function(const cli::ParseResult<MaxOptions, MaxArgs>& result);
```

Or using default template parameters:

```cpp
void handler_function(const cli::ParseResult<>& result);
```

### Accessing Parse Results

The `ParseResult` object provides several methods to access parsed data:

```cpp
void my_handler(const cli::ParseResult<>& result) {
    // 1. Check if parsing was successful
    if (!result.is_valid()) {
        // Handle error
        return;
    }

    // 2. Get the command name
    std::string_view cmd = result.command;

    // 3. Access positional arguments
    for (size_t i = 0; i < result.arg_count; i++) {
        std::string_view arg = result.args[i];
        // Use arg...
    }

    // Or use the span interface
    for (const auto& arg : result.arguments()) {
        // Use arg...
    }

    // 4. Check if an option was provided
    if (result.has_option("verbose")) {
        // Option is present
    }

    // 5. Get option value with default
    auto value = result.get_option_value("color", "default");

    // 6. Find and examine an option
    auto opt = result.find_option("brightness");
    if (opt.has_value()) {
        if (opt->is_boolean) {
            // Boolean flag
        } else {
            // Value option
            std::string_view val = opt->value;
        }
    }
}
```

### Error Handling

```cpp
auto result = cli::parse<32, 16, 8, 32>(input, registry);

if (!result.is_valid()) {
    switch (result.error) {
        case cli::ParseError::EmptyInput:
            printf("No command provided\n");
            break;
        case cli::ParseError::UnknownCommand:
            printf("Unknown command at position %zu\n", result.error_position);
            break;
        case cli::ParseError::UnknownOption:
            printf("Unknown option at position %zu\n", result.error_position);
            break;
        case cli::ParseError::MissingOptionValue:
            printf("Option requires a value\n");
            break;
        case cli::ParseError::TooFewArguments:
            printf("Too few arguments provided\n");
            break;
        case cli::ParseError::TooManyArguments:
            printf("Too many arguments provided\n");
            break;
        default:
            printf("Parse error: %s\n", cli::error_string(result.error).data());
    }
}
```

## Template Parameters

The library uses template parameters for compile-time configuration:

```cpp
cli::parse<MaxCommands, MaxOptions, MaxArgs, MaxTokens>(input, registry);
```

- `MaxCommands`: Maximum number of commands in the registry (default: 32)
- `MaxOptions`: Maximum options per command (default: 16)
- `MaxArgs`: Maximum positional arguments per command (default: 8)
- `MaxTokens`: Maximum tokens in input string (default: 32)

### Custom Configuration Example

```cpp
// Registry with 10 commands, 8 options per command, 4 args per command
constexpr auto custom_registry =
    cli::registry<10, 8, 4>()
        .add_command(/* ... */)
        .build();

// Parse with custom limits
auto result = cli::parse<10, 8, 4, 16>(input, custom_registry);
```

## Builder API

The library provides a fluent builder API for constructing commands:

### Option Builder

```cpp
auto option = cli::option("name")
    .short_flag('n')          // Add short flag
    .with_value()             // Requires a value
    .description("Help text") // Add description
    .build();

auto bool_option = cli::option("flag")
    .short_flag('f')
    .boolean()                // Boolean flag (no value)
    .description("Enable feature")
    .build();
```

### Command Builder

```cpp
auto cmd = cli::command("mycommand")
    .description("Command description")
    .exact_args(2)                    // Exactly 2 arguments
    // or .args(1, 3)                 // Between 1 and 3 arguments
    // or .min_args(1).max_args(3)    // Alternative syntax
    .add_option(option1)
    .add_option(option2)
    .handler(my_handler_function)
    .build();
```

### Registry Builder

```cpp
auto registry = cli::registry()
    .add_command(command1)
    .add_command(command2)
    .build();
```

## Best Practices

1. **Define handlers first**: Define all handler functions before creating the registry
2. **Use constexpr**: Mark registries as `constexpr` for compile-time initialization
3. **Validate input**: Always check `result.is_valid()` before accessing data
4. **Use string_view carefully**: Remember that `std::string_view` doesn't own the data - ensure the source string outlives the view
5. **Choose appropriate limits**: Set template parameters based on your actual needs to minimize memory usage
6. **Document commands**: Always provide descriptions for commands and options

## Error Codes

| Error | Description |
|-------|-------------|
| `None` | No error - successful parse |
| `EmptyInput` | Input string is empty or whitespace only |
| `UnknownCommand` | Command not found in registry |
| `UnknownOption` | Option not registered for this command |
| `MissingOptionValue` | Option requires a value but none provided |
| `InvalidOptionValue` | Option value is invalid |
| `TooFewArguments` | Fewer arguments than minimum required |
| `TooManyArguments` | More arguments than maximum allowed |
| `TooManyTokens` | Exceeded maximum token count |
| `InvalidFormat` | Malformed input (e.g., flag appears after arguments) |

## Option Ordering Rules

- **Options must come before arguments**: Once a positional argument is encountered, all remaining tokens are treated as arguments
- **Flags can be in any order**: `-a -b -c` is equivalent to `-c -b -a`
- **Value options need their value next**: `--color red` (the value immediately follows the option)

**Valid:**
```
command --opt1 value1 --opt2 value2 arg1 arg2
command --opt1 value1 arg1 arg2
```

**Invalid:**
```
command arg1 --opt1 value1  # Error: flag after argument
```

## Integration Example

Here's a complete example showing how to integrate the CLI parser into an embedded application:

```cpp
#include "cli_parser.hpp"
#include <cstdio>

// Your application handlers
void handle_get_sensor(const cli::ParseResult<>& result);
void handle_set_threshold(const cli::ParseResult<>& result);
void handle_calibrate(const cli::ParseResult<>& result);

// Global registry (constexpr for ROM storage)
constexpr auto app_registry = cli::registry<16, 8, 4>()
    .add_command(
        cli::command("get")
            .description("Read sensor value")
            .exact_args(1)
            .handler(handle_get_sensor)
    )
    .add_command(
        cli::command("set")
            .description("Set threshold")
            .exact_args(2)
            .add_option(
                cli::option("persist")
                    .short_flag('p')
                    .boolean()
                    .description("Save to EEPROM")
            )
            .handler(handle_set_threshold)
    )
    .add_command(
        cli::command("calibrate")
            .description("Calibrate sensor")
            .exact_args(0)
            .add_option(
                cli::option("timeout")
                    .short_flag('t')
                    .with_value()
                    .description("Timeout in seconds")
            )
            .handler(handle_calibrate)
    )
    .build();

// Main command processing loop
void process_command(const char* input) {
    auto result = cli::parse_and_execute<16, 8, 4, 32>(input, app_registry);

    if (!result.is_valid()) {
        printf("Error: %s\n", cli::error_string(result.error).data());
    }
}

int main() {
    char buffer[128];

    while (true) {
        printf("> ");
        if (fgets(buffer, sizeof(buffer), stdin)) {
            process_command(buffer);
        }
    }

    return 0;
}
```

## License

MIT License - Copyright (c) 2026 Rob Morley
