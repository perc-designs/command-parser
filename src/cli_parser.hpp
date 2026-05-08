/**
 * @file cli_parser.hpp
 * @brief Complete Embedded CLI Parser - Single Header (Template Parameter Version)
 *
 * Zero-overhead, compile-time configured command line parser for embedded systems.
 * This version uses template parameters instead of global constants for maximum flexibility.
 *
 * Features:
 * - No dynamic allocation (all fixed-size arrays)
 * - Compile-time command registration
 * - Runtime parsing with validation
 * - Integrated command handlers
 * - Type-safe (C++17 compatible)
 * - Zero-copy string handling
 * - Fully customizable sizes via template parameters
 *
 * Memory Usage:
 * - Flash: ~2-5 KB (depending on command count)
 * - RAM: ~512 bytes during parsing (customizable)
 *
 * @author Rob Morley
 * @date 2026-01-02
 * @version 2.1 (C++17 compatible)
 * @license MIT
 */

#ifndef CLI_PARSER_ALL_HPP
#define CLI_PARSER_ALL_HPP

#include <string_view>
#include <array>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <cctype>

#if __cplusplus >= 202002L
#include <span>
#endif

namespace cli {

// ============================================================================
// C++17 Compatibility Helpers
// ============================================================================

namespace detail {

constexpr bool starts_with(const std::string_view str, std::string_view prefix) noexcept {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

constexpr bool starts_with(const std::string_view str, const char c) noexcept {
    return !str.empty() && str[0] == c;
}

} // namespace detail

#if __cplusplus < 202002L
namespace detail {

/**
 * @brief Minimal span-like view for C++17 compatibility
 *
 * Provides a non-owning view over a contiguous sequence of elements.
 * This is a simplified version of std::span for C++17.
 */
template<typename T>
class span {
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = pointer;
    using const_iterator = const_pointer;

    constexpr span() noexcept : data_(nullptr), size_(0) {}

    constexpr span(pointer ptr, size_type count) noexcept
        : data_(ptr), size_(count) {}

    template<std::size_t N>
    constexpr span(T (&arr)[N]) noexcept
        : data_(arr), size_(N) {}

    template<std::size_t N>
    constexpr span(std::array<T, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    template<std::size_t N>
    constexpr span(const std::array<std::remove_const_t<T>, N>& arr) noexcept
        : data_(arr.data()), size_(N) {}

    constexpr iterator begin() const noexcept { return data_; }
    constexpr iterator end() const noexcept { return data_ + size_; }
    constexpr const_iterator cbegin() const noexcept { return data_; }
    constexpr const_iterator cend() const noexcept { return data_ + size_; }

    constexpr reference front() const { return *data_; }
    constexpr reference back() const { return *(data_ + size_ - 1); }
    constexpr reference operator[](size_type idx) const { return data_[idx]; }

    constexpr pointer data() const noexcept { return data_; }
    constexpr size_type size() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr span subspan(size_type offset, size_type count) const noexcept {
        return span(data_ + offset, count);
    }

    constexpr span first(size_type count) const noexcept {
        return span(data_, count);
    }

    constexpr span last(size_type count) const noexcept {
        return span(data_ + size_ - count, count);
    }

private:
    pointer data_;
    size_type size_;
};

} // namespace detail

// Export span to cli namespace for convenience
template<typename T>
using span = detail::span<T>;
#else

template<typename T>
using span = std::span<T>;

#endif

// ============================================================================
// Default Configuration (can be overridden via template parameters)
// ============================================================================

/// Default maximum number of commands
inline constexpr std::size_t DEFAULT_MAX_COMMANDS = 32;

/// Default maximum number of options per command
inline constexpr std::size_t DEFAULT_MAX_OPTIONS_PER_COMMAND = 16;

/// Default maximum number of arguments per command
inline constexpr std::size_t DEFAULT_MAX_ARGUMENTS_PER_COMMAND = 8;

/// Default maximum number of parsed tokens
inline constexpr std::size_t DEFAULT_MAX_TOKENS = 32;

// ============================================================================
// Error Types
// ============================================================================

/**
 * @brief Error codes for parsing failures
 */
enum class ParseError : uint8_t {
    None = 0,              ///< No error
    EmptyInput,            ///< Input string is empty
    UnknownCommand,        ///< Command not found in registry
    UnknownOption,         ///< Option not registered for command
    MissingOptionValue,    ///< Option requires value but none provided
    InvalidOptionValue,    ///< Option value is invalid
    TooFewArguments,       ///< Fewer arguments than minimum required
    TooManyArguments,      ///< More arguments than maximum allowed
    TooManyTokens,         ///< Exceeded maximum token count
    InvalidFormat          ///< Malformed input
};

/**
 * @brief Convert error code to human-readable string
 */
constexpr std::string_view error_string(const ParseError error) noexcept {
    switch (error) {
        case ParseError::None: return "No error";
        case ParseError::EmptyInput: return "Empty input";
        case ParseError::UnknownCommand: return "Unknown command";
        case ParseError::UnknownOption: return "Unknown option";
        case ParseError::MissingOptionValue: return "Missing option value";
        case ParseError::InvalidOptionValue: return "Invalid option value";
        case ParseError::TooFewArguments: return "Too few arguments";
        case ParseError::TooManyArguments: return "Too many arguments";
        case ParseError::TooManyTokens: return "Too many tokens";
        case ParseError::InvalidFormat: return "Invalid format";
    }
    return "Unknown error";
}

// ============================================================================
// Core Types
// ============================================================================

/**
 * @brief Represents a parsed option with its value
 */
struct ParsedOption {
    std::string_view name;   ///< Option name (long form)
    std::string_view value;  ///< Option value (empty for boolean flags)
    bool is_boolean{false};  ///< True if this is a boolean flag

    constexpr ParsedOption() noexcept = default;

    constexpr ParsedOption(const std::string_view n, const std::string_view v = "", const bool is_bool = false) noexcept
        : name(n), value(v), is_boolean(is_bool) {}

    [[nodiscard]] constexpr bool has_value() const noexcept {
        return !value.empty() || is_boolean;
    }
};

/**
 * @brief Result of the parsing operation
 *
 * @tparam MaxOptions Maximum number of options
 * @tparam MaxArgs Maximum number of arguments
 */
template<std::size_t MaxOptions = DEFAULT_MAX_OPTIONS_PER_COMMAND,
         std::size_t MaxArgs = DEFAULT_MAX_ARGUMENTS_PER_COMMAND>
struct ParseResult {
    ParseError error{ParseError::None};                   ///< Error code
    std::string_view command;                             ///< Parsed command name
    std::array<ParsedOption, MaxOptions> options{};       ///< Parsed options
    std::size_t option_count{0};                          ///< Number of valid options
    std::array<std::string_view, MaxArgs> args{};         ///< Parsed arguments
    std::size_t arg_count{0};                             ///< Number of valid arguments
    std::size_t error_position{0};                        ///< Position of error in input

    [[nodiscard]] constexpr bool is_valid() const noexcept { return error == ParseError::None; }
    constexpr explicit operator bool() const noexcept { return is_valid(); }

    /**
     * @brief Find option by name
     */
    [[nodiscard]] constexpr std::optional<ParsedOption> find_option(std::string_view name) const noexcept {
        for (std::size_t i = 0; i < option_count; ++i) {
            if (options[i].name == name) {
                return options[i];
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Check if the option exists
     */
    [[nodiscard]] constexpr bool has_option(const std::string_view name) const noexcept {
        return find_option(name).has_value();
    }

    /**
     * @brief Get option value or default
     */
    [[nodiscard]] constexpr std::string_view get_option_value(const std::string_view name,
                                                 const std::string_view default_value = "") const noexcept {
        const auto opt = find_option(name);
        return opt ? opt->value : default_value;
    }

    /**
     * @brief Get span of parsed arguments
     */
    [[nodiscard]] constexpr span<const std::string_view> arguments() const noexcept {
        return span<const std::string_view>(args.data(), arg_count);
    }
};

// ============================================================================
// Command Handler Types
// ============================================================================

/**
 * @brief Function pointer type for command handlers
 */
template<std::size_t MaxOptions = DEFAULT_MAX_OPTIONS_PER_COMMAND,
         std::size_t MaxArgs = DEFAULT_MAX_ARGUMENTS_PER_COMMAND>
using CommandHandlerFunc = void (*)(const ParseResult<MaxOptions, MaxArgs>&);

// Note: C++20 concept equivalent removed for C++17 compatibility
// The concept was:
// template<typename F, std::size_t MaxOptions, std::size_t MaxArgs>
// concept CommandHandler = requires(F f, const ParseResult<MaxOptions, MaxArgs>& result) {
//     { f(result) } -> std::same_as<void>;
// };

// ============================================================================
// Option Definition
// ============================================================================

/**
 * @brief Definition of a command option
 */
struct OptionDef {
    std::string_view long_name;   ///< Long form name (e.g., "brightness")
    char short_name{'\0'};        ///< Short form character (e.g., 'b'), or '\0' if none
    bool requires_value{true};    ///< True if option requires a value
    std::string_view description; ///< Help text description

    constexpr OptionDef() noexcept = default;

    constexpr OptionDef(std::string_view l_name,
                       char sh_name = '\0',
                       bool req_value = true,
                       std::string_view desc = "") noexcept
        : long_name(l_name)
        , short_name(sh_name)
        , requires_value(req_value)
        , description(desc) {}

    /**
     * @brief Check if this option matches a flag string
     */
    constexpr bool matches(std::string_view flag) const noexcept {
        if (detail::starts_with(flag, "--") && flag.substr(2) == long_name) {
            return true;
        }
        if (short_name != '\0' && flag.size() == 2 &&
            flag[0] == '-' && flag[1] == short_name) {
            return true;
        }
        return false;
    }

    constexpr bool is_boolean() const noexcept {
        return !requires_value;
    }
};

/**
 * @brief Argument count constraints for a command
 */
struct ArgumentSpec {
    std::size_t min_count{0};
    std::size_t max_count{0};

    constexpr ArgumentSpec() noexcept = default;
    constexpr ArgumentSpec(std::size_t min, std::size_t max) noexcept
        : min_count(min), max_count(max) {}

    [[nodiscard]] constexpr bool is_valid_count(std::size_t count) const noexcept {
        return count >= min_count && count <= max_count;
    }
};

// ============================================================================
// Command Definition
// ============================================================================

/**
 * @brief Complete definition of a command
 *
 * @tparam MaxOptions Maximum number of options for this command
 */
template<std::size_t MaxOptions = DEFAULT_MAX_OPTIONS_PER_COMMAND,
         std::size_t MaxArgs = DEFAULT_MAX_ARGUMENTS_PER_COMMAND>
struct CommandDef {
    std::string_view name;                        ///< Command name
    std::string_view description;                 ///< Help text
    std::array<OptionDef, MaxOptions> options{};  ///< Available options
    std::size_t option_count{0};                  ///< Number of valid options
    ArgumentSpec arg_spec;                        ///< Argument constraints
    CommandHandlerFunc<MaxOptions, MaxArgs> handler{nullptr};  ///< Command handler function

    constexpr CommandDef() noexcept = default;

    constexpr CommandDef(std::string_view name_sv,
                        std::string_view desc = "",
                        ArgumentSpec args = {},
                        CommandHandlerFunc<MaxOptions, MaxArgs> h = nullptr) noexcept
        : name(name_sv), description(desc), arg_spec(args), handler(h) {}

    /**
     * @brief Add an option to this command
     */
    constexpr bool add_option(const OptionDef& opt) noexcept {
        if (option_count >= MaxOptions) {
            return false;
        }
        options[option_count++] = opt;
        return true;
    }

    /**
     * @brief Find option definition by flag string
     */
    [[nodiscard]] constexpr std::optional<OptionDef> find_option(std::string_view flag) const noexcept {
        for (std::size_t i = 0; i < option_count; ++i) {
            if (options[i].matches(flag)) {
                return options[i];
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Validate argument count
     */
    [[nodiscard]] constexpr bool validate_args(std::size_t count) const noexcept {
        return arg_spec.is_valid_count(count);
    }

    /**
     * @brief Execute handler if present
     */
    void execute(const ParseResult<MaxOptions, MaxArgs>& result) const {
        if (handler) {
            handler(result);
        }
    }
};

// ============================================================================
// Command Registry
// ============================================================================

/**
 * @brief Registry of all available commands
 *
 * @tparam MaxCommands Maximum number of commands in registry
 * @tparam MaxOptions Maximum number of options per command
 * @tparam MaxArgs Maximum number of arguments per command
 */
template<std::size_t MaxCommands = DEFAULT_MAX_COMMANDS,
         std::size_t MaxOptions = DEFAULT_MAX_OPTIONS_PER_COMMAND,
         std::size_t MaxArgs = DEFAULT_MAX_ARGUMENTS_PER_COMMAND>
class CommandRegistry {
public:
    using CommandDefType = CommandDef<MaxOptions, MaxArgs>;
    using ParseResultType = ParseResult<MaxOptions, MaxArgs>;

    constexpr CommandRegistry() noexcept = default;

    /**
     * @brief Register a new command
     */
    constexpr bool register_command(const CommandDefType& cmd) noexcept {
        if (command_count_ >= MaxCommands) {
            return false;
        }
        commands_[command_count_++] = cmd;
        return true;
    }

    /**
     * @brief Find command by name
     */
    constexpr std::optional<CommandDefType> find_command(std::string_view name) const noexcept {
        for (std::size_t i = 0; i < command_count_; ++i) {
            if (commands_[i].name == name) {
                return commands_[i];
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Execute command handler by name
     */
    bool execute(const ParseResultType& result) const {
        for (std::size_t i = 0; i < command_count_; ++i) {
            if (commands_[i].name == result.command) {
                commands_[i].execute(result);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get all registered commands
     */
    constexpr span<const CommandDefType> commands() const noexcept {
        return span<const CommandDefType>(commands_.data(), command_count_);
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return command_count_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return command_count_ == 0; }

private:
    std::array<CommandDefType, MaxCommands> commands_{};
    std::size_t command_count_{0};
};

// ============================================================================
// Tokenizer
// ============================================================================

/**
 * @brief Token representing a parsed element of the command line
 */
struct Token {
    std::string_view text;
    std::size_t position;

    constexpr Token() noexcept : text(), position(0) {}
    constexpr Token(const std::string_view t, const std::size_t pos) noexcept
        : text(t), position(pos) {}

    constexpr bool empty() const noexcept { return text.empty(); }
    constexpr explicit operator bool() const noexcept { return !empty(); }

    constexpr bool is_long_flag() const noexcept {
        return detail::starts_with(text, "--") && text.size() > 2;
    }

    constexpr bool is_short_flag() const noexcept {
        return text.size() == 2 && text[0] == '-' && text[1] != '-';
    }

    constexpr bool is_flag() const noexcept {
        return is_long_flag() || is_short_flag();
    }
};

/**
 * @brief Result of tokenization
 *
 * @tparam MaxTokens Maximum number of tokens
 */
template<std::size_t MaxTokens = DEFAULT_MAX_TOKENS>
struct TokenizeResult {
    std::array<Token, MaxTokens> tokens{};
    std::size_t count{0};
    ParseError error{ParseError::None};

    constexpr bool is_valid() const noexcept { return error == ParseError::None; }
    constexpr explicit operator bool() const noexcept { return is_valid(); }

    constexpr span<const Token> get_span() const noexcept {
        return span<const Token>(tokens.data(), count);
    }
};

/**
 * @brief Tokenize command line input into tokens
 *
 * @tparam MaxTokens Maximum number of tokens to parse
 */
template<std::size_t MaxTokens = DEFAULT_MAX_TOKENS>
constexpr TokenizeResult<MaxTokens> tokenize(std::string_view input) noexcept {
    TokenizeResult<MaxTokens> result;

    if (input.empty()) {
        result.error = ParseError::EmptyInput;
        return result;
    }

    std::size_t pos = 0;
    const std::size_t len = input.size();

    // Trim leading whitespace
    while (pos < len && std::isspace(static_cast<unsigned char>(input[pos]))) {
        ++pos;
    }

    if (pos >= len) {
        result.error = ParseError::EmptyInput;
        return result;
    }

    while (pos < len && result.count < MaxTokens) {
        // Skip whitespace
        while (pos < len && std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }

        if (pos >= len) {
            break;
        }

        // Find end of token
        std::size_t token_start = pos;

        while (pos < len && !std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }

        std::size_t token_len = pos - token_start;
        std::string_view token_text = input.substr(token_start, token_len);

        result.tokens[result.count++] = Token(token_text, token_start);
    }

    // Check if we exceeded token limit
    if (pos < len) {
        while (pos < len && std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }
        if (pos < len) {
            result.error = ParseError::TooManyTokens;
        }
    }

    return result;
}

/**
 * @brief Validate token structure
 */
template<std::size_t MaxTokens>
constexpr bool validate_tokens(const TokenizeResult<MaxTokens>& tokens) noexcept {
    if (!tokens.is_valid() || tokens.count == 0) {
        return false;
    }

    if (tokens.tokens[0].is_flag()) {
        return false;
    }

    return true;
}

// ============================================================================
// Parser Implementation
// ============================================================================

/**
 * @brief Parse state machine states
 */
enum class ParseState {
    ExpectCommand,
    ExpectOptionOrArg,
    ExpectOptionValue
};

/**
 * @brief Internal parser state
 */
struct ParserState {
    ParseState state{ParseState::ExpectCommand};
    std::optional<OptionDef> pending_option;
    std::size_t pending_option_position{0};
    bool saw_option{false};
};

/**
 * @brief Parse command line input
 *
 * @tparam MaxCommands Maximum commands in registry
 * @tparam MaxOptions Maximum options per command
 * @tparam MaxArgs Maximum arguments per command
 * @tparam MaxTokens Maximum tokens to parse
 */
template<std::size_t MaxCommands,
         std::size_t MaxOptions,
         std::size_t MaxArgs,
         std::size_t MaxTokens>
constexpr ParseResult<MaxOptions, MaxArgs> parse(
    std::string_view input,
    const CommandRegistry<MaxCommands, MaxOptions, MaxArgs>& registry) noexcept {

    ParseResult<MaxOptions, MaxArgs> result;

    // Step 1: Tokenize
    auto tokens = tokenize<MaxTokens>(input);

    if (!tokens.is_valid()) {
        result.error = tokens.error;
        return result;
    }

    if (tokens.count == 0) {
        result.error = ParseError::EmptyInput;
        return result;
    }

    if (!validate_tokens(tokens)) {
        result.error = ParseError::InvalidFormat;
        return result;
    }

    // Step 2: Extract command
    result.command = tokens.tokens[0].text;

    auto cmd_def = registry.find_command(result.command);
    if (!cmd_def) {
        result.error = ParseError::UnknownCommand;
        result.error_position = tokens.tokens[0].position;
        return result;
    }

    // Step 3: Parse options and arguments
    ParserState state;
    state.state = ParseState::ExpectOptionOrArg;

    for (std::size_t i = 1; i < tokens.count; ++i) {
        const auto& token = tokens.tokens[i];

        switch (state.state) {
            case ParseState::ExpectCommand:
                result.error = ParseError::InvalidFormat;
                return result;

            case ParseState::ExpectOptionOrArg: {
                if (token.is_flag()) {
                    auto opt_def = cmd_def->find_option(token.text);

                    if (!opt_def) {
                        result.error = ParseError::UnknownOption;
                        result.error_position = token.position;
                        return result;
                    }

                    state.saw_option = true;

                    if (opt_def->is_boolean()) {
                        if (result.option_count >= MaxOptions) {
                            result.error = ParseError::TooManyArguments;
                            return result;
                        }

                        result.options[result.option_count++] =
                            ParsedOption(opt_def->long_name, "", true);
                    } else {
                        state.pending_option = opt_def;
                        state.pending_option_position = token.position;
                        state.state = ParseState::ExpectOptionValue;
                    }
                } else {
                    if (state.saw_option) {
                        result.error = ParseError::InvalidFormat;
                        result.error_position = token.position;
                        return result;
                    }

                    if (result.arg_count >= MaxArgs) {
                        result.error = ParseError::TooManyArguments;
                        result.error_position = token.position;
                        return result;
                    }

                    result.args[result.arg_count++] = token.text;
                }
                break;
            }

            case ParseState::ExpectOptionValue: {
                if (token.is_flag()) {
                    result.error = ParseError::MissingOptionValue;
                    result.error_position = state.pending_option_position;
                    return result;
                }

                if (result.option_count >= MaxOptions) {
                    result.error = ParseError::TooManyArguments;
                    return result;
                }

                result.options[result.option_count++] =
                    ParsedOption(state.pending_option->long_name, token.text, false);

                state.pending_option.reset();
                state.state = ParseState::ExpectOptionOrArg;
                break;
            }
        }
    }

    // Step 4: Validate final state
    if (state.state == ParseState::ExpectOptionValue) {
        result.error = ParseError::MissingOptionValue;
        result.error_position = state.pending_option_position;
        return result;
    }

    // Step 5: Validate argument count
    if (!cmd_def->validate_args(result.arg_count)) {
        if (result.arg_count < cmd_def->arg_spec.min_count) {
            result.error = ParseError::TooFewArguments;
        } else {
            result.error = ParseError::TooManyArguments;
        }
        return result;
    }

    return result;
}

/**
 * @brief Parse and execute command using registry handlers
 */
template<std::size_t MaxCommands,
         std::size_t MaxOptions,
         std::size_t MaxArgs,
         std::size_t MaxTokens>
ParseResult<MaxOptions, MaxArgs> parse_and_execute(
    std::string_view input,
    const CommandRegistry<MaxCommands, MaxOptions, MaxArgs>& registry) {

    auto result = parse<MaxCommands, MaxOptions, MaxArgs, MaxTokens>(input, registry);

    if (result.is_valid()) {
        registry.execute(result);
    }

    return result;
}

// ============================================================================
// Builder API
// ============================================================================

/**
 * @brief Builder for creating OptionDef instances
 */
class OptionBuilder {
public:
    constexpr OptionBuilder(std::string_view long_name) noexcept
        : def_(long_name) {}

    constexpr OptionBuilder& short_flag(char c) noexcept {
        def_.short_name = c;
        return *this;
    }

    constexpr OptionBuilder& boolean() noexcept {
        def_.requires_value = false;
        return *this;
    }

    constexpr OptionBuilder& description(std::string_view desc) noexcept {
        def_.description = desc;
        return *this;
    }

    constexpr OptionBuilder& with_value() noexcept {
        def_.requires_value = true;
        return *this;
    }

    constexpr OptionDef build() const noexcept {
        return def_;
    }

    constexpr operator OptionDef() const noexcept {
        return def_;
    }

private:
    OptionDef def_;
};

constexpr OptionBuilder option(std::string_view long_name) noexcept {
    return OptionBuilder(long_name);
}

/**
 * @brief Builder for creating CommandDef instances
 */
template<std::size_t MaxOptions = DEFAULT_MAX_OPTIONS_PER_COMMAND,
         std::size_t MaxArgs = DEFAULT_MAX_ARGUMENTS_PER_COMMAND>
class CommandBuilder {
public:
    using CommandDefType = CommandDef<MaxOptions, MaxArgs>;
    using HandlerType = CommandHandlerFunc<MaxOptions, MaxArgs>;

    constexpr CommandBuilder(std::string_view name) noexcept {
        cmd_.name = name;
    }

    constexpr CommandBuilder& description(std::string_view desc) noexcept {
        cmd_.description = desc;
        return *this;
    }

    constexpr CommandBuilder& handler(HandlerType h) noexcept {
        cmd_.handler = h;
        return *this;
    }

    constexpr CommandBuilder& add_option(const OptionDef& opt) noexcept {
        cmd_.add_option(opt);
        return *this;
    }

    constexpr CommandBuilder& add_option(const OptionBuilder& builder) noexcept {
        return add_option(builder.build());
    }

    constexpr CommandBuilder& min_args(std::size_t count) noexcept {
        cmd_.arg_spec.min_count = count;
        return *this;
    }

    constexpr CommandBuilder& max_args(std::size_t count) noexcept {
        cmd_.arg_spec.max_count = count;
        return *this;
    }

    constexpr CommandBuilder& args(std::size_t min, std::size_t max) noexcept {
        cmd_.arg_spec.min_count = min;
        cmd_.arg_spec.max_count = max;
        return *this;
    }

    constexpr CommandBuilder& exact_args(std::size_t count) noexcept {
        return args(count, count);
    }

    constexpr CommandDefType build() const noexcept {
        return cmd_;
    }

    constexpr operator CommandDefType() const noexcept {
        return cmd_;
    }

private:
    CommandDefType cmd_;
};

template<std::size_t MaxOptions = DEFAULT_MAX_OPTIONS_PER_COMMAND,
         std::size_t MaxArgs = DEFAULT_MAX_ARGUMENTS_PER_COMMAND>
constexpr CommandBuilder<MaxOptions, MaxArgs> command(std::string_view name) noexcept {
    return CommandBuilder<MaxOptions, MaxArgs>(name);
}

/**
 * @brief Builder for creating CommandRegistry
 */
template<std::size_t MaxCommands = DEFAULT_MAX_COMMANDS,
         std::size_t MaxOptions = DEFAULT_MAX_OPTIONS_PER_COMMAND,
         std::size_t MaxArgs = DEFAULT_MAX_ARGUMENTS_PER_COMMAND>
class RegistryBuilder {
public:
    using RegistryType = CommandRegistry<MaxCommands, MaxOptions, MaxArgs>;
    using CommandDefType = CommandDef<MaxOptions, MaxArgs>;

    constexpr RegistryBuilder() noexcept = default;

    constexpr RegistryBuilder& add_command(const CommandDefType& cmd) noexcept {
        registry_.register_command(cmd);
        return *this;
    }

    constexpr RegistryBuilder& add_command(const CommandBuilder<MaxOptions, MaxArgs>& builder) noexcept {
        return add_command(builder.build());
    }

    constexpr RegistryType build() const noexcept {
        return registry_;
    }

    constexpr operator RegistryType() const noexcept {
        return registry_;
    }

private:
    RegistryType registry_;
};

template<std::size_t MaxCommands = DEFAULT_MAX_COMMANDS,
         std::size_t MaxOptions = DEFAULT_MAX_OPTIONS_PER_COMMAND,
         std::size_t MaxArgs = DEFAULT_MAX_ARGUMENTS_PER_COMMAND>
constexpr RegistryBuilder<MaxCommands, MaxOptions, MaxArgs> registry() noexcept {
    return RegistryBuilder<MaxCommands, MaxOptions, MaxArgs>();
}

} // namespace cli

#endif // CLI_PARSER_ALL_HPP
