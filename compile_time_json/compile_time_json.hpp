#include <cstddef>
#include <ranges>
#include <algorithm>
#include <array>
#include <variant>
#include <span>
#include <iostream>
#include <charconv>
#include <type_traits>
#include <limits>
#include <ranges>

template <std::size_t... Indices>
struct EnumeratorImpl
{
    template <template <std::size_t> typename ElementHolder, template <typename...> typename DestinationRange>
    using Enumerate = DestinationRange<ElementHolder<Indices>...>;
};

template <std::size_t... Indices>
constexpr EnumeratorImpl<Indices...> create_enumerater(const std::index_sequence<Indices...> &);

template <template <std::size_t> typename ElementHolder, template <typename...> typename DestinationRange, std::size_t N>
using Enumerate = typename decltype(create_enumerater(std::make_index_sequence<N>{}))::template Enumerate<ElementHolder, DestinationRange>;

template <std::size_t N>
struct FixedLengthString
{
    std::array<char, N> string{};

    constexpr FixedLengthString(char const (&i_string)[N])
    {
        std::copy(std::begin(i_string), std::end(i_string), string.begin());
    }

    explicit constexpr FixedLengthString(const char *const i_string)
    {
        std::copy(i_string, i_string + N, string.begin());
    }
};

template <auto IValue>
struct CompileTimeValueHolder
{
    static constexpr auto Value = IValue;
};

enum struct JsonValueType
{
    BOOL,
    SIGNED_INTEGER,
    UNSIGNED_INTEGER,
    DOUBLE,
    STRING,
    OBJECT,
    ARRAY,
    NULL_VALUE,
};

template <FixedLengthString String>
struct ParseContext
{
    struct WholeInputString
    {
    };

    static constexpr std::size_t EstimatedMaxResultSize = String.string.size() / 6;
    struct StringView
    {
        std::size_t begin{};
        std::size_t end{};

        StringView() noexcept = default;

        explicit constexpr StringView(const WholeInputString &) noexcept : begin{}, end(String.string.size())
        {
        }

        explicit constexpr StringView(const std::string_view &view) noexcept : begin(view.data() - String.string.data()), end(begin + view.size())
        {
        }

        constexpr std::size_t find() const noexcept
        {
            return 0;
        }

        explicit constexpr operator std::string_view() const
        {
            return {String.string.data() + begin, String.string.data() + end};
        }
    };

    struct JsonMember
    {
        StringView name;
        JsonValueType type = JsonValueType::NULL_VALUE;
        StringView value;
        std::size_t object_start{0};
        std::size_t member_count{0};

        constexpr auto get_bool() const
        {
            return std::string_view{value} == "true";
        }

        constexpr auto get_signed_integer() const
        {
            const std::string_view value_string{value};
            std::intmax_t int_value = 0;

            if (std::is_constant_evaluated()) // TODO remove when from_chars gets constexpr
            {
                for (const auto c : value_string.substr(1))
                {
                    const auto digit = c - '0';
                    if ((std::numeric_limits<std::intmax_t>::min() + digit) / 10 > int_value)
                        break;

                    int_value = int_value * 10 - digit;
                }
            }
            else
                std::from_chars(value_string.begin(), value_string.end(), int_value);

            return int_value;
        }

        constexpr auto get_unsigned_integer() const
        {
            const std::string_view value_string{value};
            std::uintmax_t uint_value = 0;

            if (std::is_constant_evaluated()) // TODO: remove when from_chars gets constexpr
            {
                for (const auto c : value_string)
                {
                    const auto digit = c - '0';
                    if ((std::numeric_limits<std::intmax_t>::max() - digit) / 10 < uint_value)
                        break;

                    uint_value = uint_value * 10 + digit;
                }
            }
            else
                std::from_chars(value_string.begin(), value_string.end(), uint_value);

            return uint_value;
        }

        constexpr auto get_double() const
        {
            std::string_view value_string{value};
            double double_value = 0;

            if (std::is_constant_evaluated()) // TODO: remove when from_chars gets constexpr or improve this part
            {
                double sign = 1.0;
                if (value_string.front() == '-')
                {
                    value_string = value_string.substr(1);
                    sign = -1;
                }

                const auto dot_pos = value_string.find('.');

                const auto whole_string = value_string.substr(0, dot_pos);
                const auto fraction_string = dot_pos != std::string_view::npos ? value_string.substr(dot_pos + 1) : std::string_view{};

                double whole_value = 0.0;
                for (const auto c : whole_string)
                {
                    const auto digit = c - '0';
                    whole_value = whole_value * 10.0 + digit;
                }

                double fraction_value = 0.0;
                for (const auto c : std::ranges::reverse_view(fraction_string))
                {
                    const auto digit = c - '0';
                    fraction_value = (fraction_value + digit) / 10.0;
                }

                double_value = whole_value + fraction_value;
            }
            else
                std::from_chars(value_string.begin(), value_string.end(), double_value);
            return double_value;
        }

        constexpr auto get_string() const // TODO: wait for string to become constexpr in implementation
        {
            const std::string_view value_string{value};
            std::string string_value;
            string_value.reserve(value_string.size());

            bool is_in_escape_state = false;
            for (const char c : value_string)
            {
                if (is_in_escape_state)
                {
                    switch (c)
                    {
                    case 'n':
                        string_value.push_back('\n');
                        break;
                    case 'r':
                        string_value.push_back('\r');
                        break;
                    case 'b':
                        string_value.push_back('\b');
                        break;
                    case 'f':
                        string_value.push_back('\f');
                        break;
                    case 't':
                        string_value.push_back('\t');
                        break;
                    case '"':
                        string_value.push_back('"');
                        break;
                    case '\\':
                        string_value.push_back('\\');
                        break;
                    default:
                        string_value.push_back('\\');
                        string_value.push_back(c);
                    }
                    is_in_escape_state = false;
                }
                else
                {
                    if (c == '\\')
                    {
                        is_in_escape_state = true;
                        continue;
                    }

                    string_value.push_back(c);
                }
            }
            return string_value;
        }
    };

    struct FinalJsonResultContext
    {
        std::span<JsonMember> children;
        std::size_t &children_count;
        std::span<JsonMember> children_object_values;
        std::size_t &children_object_values_length;
    };

    struct SuccessResult
    {
        std::string_view remaining;
    };

    struct FailureResult
    {
        std::string_view error;
        std::size_t char_index;
    };

    using ResultType = std::variant<SuccessResult, FailureResult>;

    using ParserType = std::function<ResultType(const std::string_view &input, const FinalJsonResultContext final_result_context)>;

    static constexpr bool is_white_space(const char c)
    {
        return c == ' ' | c == '\t' | c == '\n';
    }

    static constexpr bool is_alphabet(const char c)
    {
        return (c <= 'z' & c >= 'a') | (c <= 'Z' & c >= 'A');
    }

    static constexpr bool is_digit(const char c)
    {
        return (c <= '9' & c >= '0');
    }

    template <typename Parser>
    static constexpr auto zero_or_many(Parser &&parser)
    {
        return [parser](const std::string_view &input, const FinalJsonResultContext final_result_context) {
            auto remaining = input;
            while (true)
            {
                const auto result = parser(remaining, final_result_context);
                if (const auto *const success_result = std::get_if<SuccessResult>(&result))
                    remaining = success_result->remaining;
                else
                    return ResultType{SuccessResult{remaining}};
            }
        };
    }

    template <typename Parser>
    static constexpr auto at_most_one(Parser &&parser)
    {
        return [parser](const std::string_view &input, const FinalJsonResultContext final_result_context) {
            auto remaining = input;
            const auto result = parser(remaining, final_result_context);
            if (const auto *const success_result = std::get_if<SuccessResult>(&result))
                return result;
            else
                return ResultType{SuccessResult{remaining}};
        };
    }

    template <typename HeadParser, typename... TailParsers>
    static constexpr ResultType fold_execute_parsers(const std::string_view &input, const FinalJsonResultContext final_result_context, HeadParser &&head_parser, TailParsers &&... tail_parsers)
    {
        const auto head_result = head_parser(input, final_result_context);

        if (const auto *const success_result = std::get_if<SuccessResult>(&head_result))
        {
            if constexpr (sizeof...(TailParsers))
                return fold_execute_parsers(success_result->remaining, final_result_context, tail_parsers...);
            else
                return head_result;
        }
        else
            return head_result;
    }

    template <typename... Parsers>
    static constexpr auto concat_parsers(Parsers &&... parsers)
    {
        return [parsers...](const std::string_view &input, const FinalJsonResultContext final_result_context) {
            return fold_execute_parsers(input, final_result_context, parsers...);
        };
    }

    template <typename HeadParser, typename... TailParsers>
    static constexpr ResultType execute_until_parsers(const std::string_view &input, const FinalJsonResultContext final_result_context, HeadParser &&head_parser, TailParsers &&... tail_parsers)
    {
        const auto head_result = head_parser(input, final_result_context);

        if (const auto *const success_result = std::get_if<SuccessResult>(&head_result))
            return head_result;
        else
        {
            if constexpr (sizeof...(TailParsers))
                return execute_until_parsers(input, final_result_context, tail_parsers...);
            else
                return head_result;
        }
    }

    template <typename... Parsers>
    static constexpr auto any_of_parsers(Parsers &&... parsers)
    {
        return [parsers...](const std::string_view &input, const FinalJsonResultContext final_result_context) {
            return execute_until_parsers(input, final_result_context, parsers...);
        };
    }

    template <typename Predicate>
    static constexpr auto single_character_predicate_parser(Predicate &&predicate)
    {
        return [predicate](const std::string_view &input, const FinalJsonResultContext final_result_context) {
            if (input.size() && predicate(input.front()))
                return ResultType{SuccessResult{input.substr(1)}};
            else
                return ResultType{FailureResult{"The character failed the expected predicate.", 0}};
        };
    }

    static constexpr auto single_character_parser(const char c)
    {
        return single_character_predicate_parser([c](const char input) { return c == input; });
    }

    static constexpr auto ZeroOrManySpaces = zero_or_many(single_character_predicate_parser(is_white_space));
    static constexpr auto ParseQuote = single_character_parser('\"');
    static constexpr auto ParseComma = single_character_parser(',');
    static constexpr auto ParseMinus = single_character_parser('-');
    static constexpr auto ParseDot = single_character_parser('.');
    static constexpr auto ParseUnsignedInteger = concat_parsers(single_character_predicate_parser(is_digit), zero_or_many(single_character_predicate_parser(is_digit)));

    static constexpr ResultType parse_value(const std::string_view &input, const FinalJsonResultContext final_result_context)
    {
        auto &current_member = final_result_context.children[final_result_context.children_count];

        {
            constexpr auto ParseNull = concat_parsers(
                single_character_parser('n'),
                single_character_parser('u'),
                single_character_parser('l'),
                single_character_parser('l'));
            const auto parse_null_result = ParseNull(input, final_result_context);

            if (const auto *const success_result = std::get_if<SuccessResult>(&parse_null_result))
            {
                current_member.type = JsonValueType::NULL_VALUE;
                return parse_null_result;
            }
        }

        {
            constexpr auto ParseBool = any_of_parsers(
                concat_parsers(
                    single_character_parser('t'),
                    single_character_parser('r'),
                    single_character_parser('u'),
                    single_character_parser('e')),
                concat_parsers(
                    single_character_parser('f'),
                    single_character_parser('a'),
                    single_character_parser('l'),
                    single_character_parser('s'),
                    single_character_parser('e')));
            const auto parse_bool_result = ParseBool(input, final_result_context);

            if (const auto *const success_result = std::get_if<SuccessResult>(&parse_bool_result))
            {
                current_member.type = JsonValueType::BOOL;
                current_member.value = StringView{std::string_view(input.data(), success_result->remaining.data())};
                return parse_bool_result;
            }
        }

        {
            current_member.object_start = final_result_context.children_object_values_length;
            std::array<JsonMember, EstimatedMaxResultSize> children_object_values;
            std::size_t children_object_values_length = 0;
            const FinalJsonResultContext new_value_parse_result_context{final_result_context.children_object_values, final_result_context.children_object_values_length, children_object_values, children_object_values_length};
            const auto parse_json_result = parse_json_impl(input, new_value_parse_result_context);

            if (const auto *const success_result = std::get_if<SuccessResult>(&parse_json_result))
            {
                current_member.type = JsonValueType::OBJECT;
                current_member.member_count = final_result_context.children_object_values_length - current_member.object_start;

                std::ranges::copy(std::span(children_object_values.begin(), children_object_values_length), final_result_context.children_object_values.begin() + final_result_context.children_object_values_length);
                final_result_context.children_object_values_length += children_object_values_length;
                return parse_json_result;
            }
        }

        {
            constexpr auto ParseString = concat_parsers(
                ParseQuote,
                zero_or_many(any_of_parsers(
                    concat_parsers(single_character_parser('\\'), any_of_parsers(single_character_parser('\\'), ParseQuote)),
                    single_character_predicate_parser([](const char c) { return (c != '\t') & (c != '\n') & (c != '\b') & (c != '\f') & (c != '\r') & (c != '"'); }))),
                ParseQuote);

            const auto parse_string_result = ParseString(input, final_result_context);

            if (const auto *const success_result = std::get_if<SuccessResult>(&parse_string_result))
            {
                current_member.type = JsonValueType::STRING;
                current_member.value = StringView{std::string_view(std::next(input.data()), std::prev(success_result->remaining.data()))};
                return parse_string_result;
            }
        }

        {
            constexpr auto ParseArrayMember = [](const std::string_view &input, const FinalJsonResultContext final_result_context) {
                const auto parse_value_result = parse_value(input, final_result_context);
                if (const auto *const success_result = std::get_if<SuccessResult>(&parse_value_result))
                    final_result_context.children_count++;

                return parse_value_result;
            };
            constexpr auto ParseArray = concat_parsers(
                single_character_parser('['),
                ZeroOrManySpaces,
                at_most_one(
                    concat_parsers(
                        ParseArrayMember,
                        ZeroOrManySpaces,
                        zero_or_many(concat_parsers(ParseComma, ZeroOrManySpaces, ParseArrayMember, ZeroOrManySpaces)),
                        at_most_one(concat_parsers(ParseComma, ZeroOrManySpaces)))),
                single_character_parser(']'));

            current_member.object_start = final_result_context.children_object_values_length;
            std::array<JsonMember, EstimatedMaxResultSize> children_object_values;
            std::size_t children_object_values_length = 0;
            const FinalJsonResultContext new_value_parse_result_context{final_result_context.children_object_values, final_result_context.children_object_values_length, children_object_values, children_object_values_length};
            const auto parse_array_result = ParseArray(input, new_value_parse_result_context);

            if (const auto *const success_result = std::get_if<SuccessResult>(&parse_array_result))
            {
                current_member.type = JsonValueType::ARRAY;
                current_member.member_count = final_result_context.children_object_values_length - current_member.object_start;

                std::ranges::copy(std::span(children_object_values.begin(), children_object_values_length), final_result_context.children_object_values.begin() + final_result_context.children_object_values_length);
                final_result_context.children_object_values_length += children_object_values_length;
                return parse_array_result;
            }
        }

        {
            constexpr auto ParseFloatingPointStartingWithDot = concat_parsers(at_most_one(ParseMinus), ParseDot, ParseUnsignedInteger);

            const auto parse_floating_point_starting_with_dot_result = ParseFloatingPointStartingWithDot(input, final_result_context);

            if (const auto *const success_result = std::get_if<SuccessResult>(&parse_floating_point_starting_with_dot_result))
            {
                current_member.type = JsonValueType::DOUBLE;
                current_member.value = StringView{std::string_view(input.data(), success_result->remaining.data())};
                return parse_floating_point_starting_with_dot_result;
            }
        }

        {
            // TODO: Improve parsing to include scientific notation and lots more
            constexpr auto ParseFloatingPoint = concat_parsers(at_most_one(ParseMinus), ParseUnsignedInteger, ParseDot, zero_or_many(single_character_predicate_parser(is_digit)));

            const auto parse_floating_point_result = ParseFloatingPoint(input, final_result_context);

            if (const auto *const success_result = std::get_if<SuccessResult>(&parse_floating_point_result))
            {
                current_member.type = JsonValueType::DOUBLE;
                current_member.value = StringView{std::string_view(input.data(), success_result->remaining.data())};
                return parse_floating_point_result;
            }
        }

        {
            const auto parse_integer_result = ParseUnsignedInteger(input, final_result_context);

            if (const auto *const success_result = std::get_if<SuccessResult>(&parse_integer_result))
            {
                current_member.type = JsonValueType::UNSIGNED_INTEGER;
                current_member.value = StringView{std::string_view(input.data(), success_result->remaining.data())};
                return parse_integer_result;
            }
        }

        {
            constexpr auto ParseSignedInteger = concat_parsers(ParseMinus, ParseUnsignedInteger);
            const auto parse_integer_result = ParseSignedInteger(input, final_result_context);

            if (const auto *const success_result = std::get_if<SuccessResult>(&parse_integer_result))
            {
                current_member.type = JsonValueType::SIGNED_INTEGER;
                current_member.value = StringView{std::string_view(input.data(), success_result->remaining.data())};
                return parse_integer_result;
            }
        }

        return ResultType{FailureResult{"Expected a value but couldn't find any.", 0}};
    }

    static constexpr ResultType parse_member(const std::string_view &input, const FinalJsonResultContext final_result_context)
    {
        constexpr auto IdentifierParser = concat_parsers(single_character_predicate_parser(is_alphabet), zero_or_many(single_character_predicate_parser([](const char c) { return is_alphabet(c) | is_digit(c) | c == '_'; })));
        constexpr auto ParseName = concat_parsers(ParseQuote, IdentifierParser, ParseQuote);

        const auto parse_name_result = ParseName(input, final_result_context);
        if (const auto *const failure_result = std::get_if<FailureResult>(&parse_name_result))
            return parse_name_result;

        const auto parse_name_remaining = std::get<SuccessResult>(parse_name_result).remaining;
        final_result_context.children[final_result_context.children_count].name = StringView{std::string_view{std::next(input.data()), std::prev(parse_name_remaining.data())}};

        constexpr auto ParseUntilValue = concat_parsers(ZeroOrManySpaces, single_character_parser(':'), ZeroOrManySpaces);
        const auto parse_until_value_result = ParseUntilValue(parse_name_remaining, final_result_context);
        if (const auto *const failure_result = std::get_if<FailureResult>(&parse_until_value_result))
            return parse_until_value_result;

        const auto parse_value_result = parse_value(std::get<SuccessResult>(parse_until_value_result).remaining, final_result_context);
        if (const auto *const failure_result = std::get_if<FailureResult>(&parse_value_result))
            return parse_value_result;

        final_result_context.children_count++;
        return parse_value_result;
    }

    static constexpr ResultType parse_json_impl(const std::string_view &input, const FinalJsonResultContext final_result_context)
    {
        return concat_parsers(ZeroOrManySpaces,
                              single_character_parser('{'), ZeroOrManySpaces,
                              at_most_one(concat_parsers(parse_member, ZeroOrManySpaces,
                                                         zero_or_many(concat_parsers(ParseComma, ZeroOrManySpaces, parse_member, ZeroOrManySpaces)),
                                                         at_most_one(concat_parsers(ParseComma, ZeroOrManySpaces)))),
                              single_character_parser('}'))(input, final_result_context);
    }

    struct JsonStructure
    {
        using JsonMemberType = JsonMember;
        std::array<JsonMember, EstimatedMaxResultSize> members;
        std::size_t children_count = 0;
    };

    static constexpr JsonStructure parse_json()
    {
        std::array<JsonMember, EstimatedMaxResultSize> children;
        std::size_t children_count = 0;
        std::array<JsonMember, EstimatedMaxResultSize> children_object_values;
        std::size_t children_object_values_length = 0;

        FinalJsonResultContext result_context{children, children_count, children_object_values, children_object_values_length};
        const auto parse_result = parse_json_impl(std::string_view(StringView{WholeInputString{}}), result_context);

        std::ranges::copy(std::span(children_object_values.begin(), children_object_values_length), children.begin() + children_count);

        if (const auto *failure_result = std::get_if<FailureResult>(&parse_result))
            throw *failure_result;

        return {children, children_count};
    }
};

template <JsonValueType Type>
struct Member
{
    struct Void
    {
    };
    Void value;

    constexpr Member() noexcept = default;
    constexpr Member(const auto &, const auto &json_member)
    {
    }
};

template <>
struct Member<JsonValueType::BOOL>
{
    bool value;
    constexpr Member() noexcept = default;
    constexpr Member(const auto &, const auto &json_member) : value(json_member.get_bool())
    {
    }
};

template <>
struct Member<JsonValueType::SIGNED_INTEGER>
{
    std::intmax_t value;

    constexpr Member() noexcept = default;
    constexpr Member(const auto &, const auto &json_member) : value(json_member.get_signed_integer())
    {
    }
};

template <>
struct Member<JsonValueType::UNSIGNED_INTEGER>
{
    std::uintmax_t value;

    constexpr Member() noexcept = default;
    constexpr Member(const auto &, const auto &json_member) : value(json_member.get_unsigned_integer())
    {
    }
};

template <>
struct Member<JsonValueType::DOUBLE>
{
    double value;

    constexpr Member() noexcept = default;
    constexpr Member(const auto &, const auto &json_member) : value(json_member.get_double())
    {
    }
};

template <>
struct Member<JsonValueType::STRING>
{
    std::string value;

    Member() noexcept = default; // Make constexpr when possible
    constexpr Member(const auto &, const auto &json_member) : value(json_member.get_string())
    {
    }
};

template <FixedLengthString Name, typename Value>
struct NamedValue
{
    Value value;
    template <typename... Args>
    constexpr NamedValue(Args &&... args) : value(std::forward<Args>(args)...)
    {
    }
};

template <std::size_t Index, typename Value>
struct IndexedValue : Value
{
    Value value;
    template <typename... Args>
    constexpr IndexedValue(Args &&... args) : value(std::forward<Args>(args)...)
    {
    }
};

template <typename... Members>
struct Array : Members...
{
    constexpr Array() noexcept = default;

    template <std::size_t... Indices>
    constexpr Array(const auto &json_members, const auto &json_member, const std::index_sequence<Indices...> &) : Members(std::ranges::subrange(json_members.begin() + json_member.object_start + json_member.member_count, json_members.end()), json_members[json_member.object_start + Indices])...
    {
    }

    constexpr Array(const auto &json_members, const auto &json_member) : Array(json_members, json_member, std::make_index_sequence<sizeof...(Members)>())
    {
    }

    template <std::size_t Index, typename ValueType>
    static constexpr auto &get_impl(IndexedValue<Index, ValueType> &member)
    {
        return member.value;
    }

    template <std::size_t Index>
    constexpr decltype(auto) get()
    {
        return get_impl<Index>(*this);
    }

    template <std::size_t Index, typename ValueType>
    static constexpr const auto &get_impl(const IndexedValue<Index, ValueType> &member)
    {
        return member.value;
    }

    template <std::size_t Index>
    constexpr decltype(auto) get() const
    {
        return get_impl<Index>(*this);
    }
};

template <typename... Members>
struct Json : Members...
{
    constexpr Json() noexcept = default;

    template <std::size_t... Indices>
    constexpr Json(const auto &json_members, const std::size_t object_start, const std::size_t member_count, const std::index_sequence<Indices...> &) : Members(std::ranges::subrange(json_members.begin() + object_start + member_count, json_members.end()), json_members[object_start + Indices])...
    {
    }

    constexpr Json(const auto &json_members, const std::size_t object_start, const std::size_t member_count) : Json(json_members, object_start, member_count, std::make_index_sequence<sizeof...(Members)>())
    {
    }

    constexpr Json(const auto &json_members, const auto &json_member) : Json(json_members, json_member.object_start, json_member.member_count)
    {
    }

    static constexpr void get_impl(const auto&)
    {
        throw "InvalidMemberAccess";
    }

    template <FixedLengthString Name, typename ValueType>
    static constexpr auto &get_impl(NamedValue<Name, ValueType> &member)
    {
        return member.value;
    }

    template <auto Name>
    constexpr decltype(auto) get()
    {
        return ((get_impl<Name.Value>(*this)));
    }

    template <FixedLengthString Name>
    constexpr decltype(auto) operator[](const CompileTimeValueHolder<Name>&)
    {
        return ((get_impl<Name>(*this)));
    }

    template <FixedLengthString Name, typename ValueType>
    static constexpr const auto &get_impl(const NamedValue<Name, ValueType> &member)
    {
        return member.value;
    }

    template <auto Name>
    constexpr decltype(auto) get() const
    {
        return ((get_impl<Name.Value>(*this)));
    }

    template <FixedLengthString Name>
    constexpr decltype(auto) operator[](const CompileTimeValueHolder<Name>&) const
    {
        return ((get_impl<Name>(*this)));
    }
};

template <JsonValueType ValueType, typename>
struct MemberTypeSelector
{
    using MemberType = Member<ValueType>;
};

template <typename StructureMembersView>
struct MemberTypeSelector<JsonValueType::OBJECT, StructureMembersView>
{
    template <std::size_t, typename StructureMembersView::JsonMemberType Member>
    using ChildMemberType = NamedValue<FixedLengthString<Member.name.end - Member.name.begin>{std::string_view{Member.name}.data()},
                                        typename MemberTypeSelector<Member.type, typename StructureMembersView::template NextViewSubView<Member.object_start, Member.member_count>>::MemberType>;
    using MemberType = typename StructureMembersView::template EnumerateView<ChildMemberType, Json>;
};

template <typename StructureMembersView>
struct MemberTypeSelector<JsonValueType::ARRAY, StructureMembersView>
{
    template <std::size_t Index, typename  StructureMembersView::JsonMemberType Member>
    using ChildMemberType = IndexedValue<Index,
                                        typename MemberTypeSelector<Member.type, typename StructureMembersView::template NextViewSubView<Member.object_start, Member.member_count>>::MemberType>;
    using MemberType = typename StructureMembersView::template EnumerateView<ChildMemberType, Array>;
};

template <auto JsonEarlyStructure>
struct JsonStructureContext
{
    template <std::size_t Begin, std::size_t MemberCount>
    struct View
    {
        using JsonMemberType = typename  decltype(JsonEarlyStructure)::JsonMemberType;
        template <template <std::size_t, JsonMemberType> typename ElementHolder>
        struct Zipper
        {
            template <std::size_t Index>
            using Zipped = ElementHolder<Index, JsonEarlyStructure.members[Begin + Index]>;
        };

        template <template <std::size_t, JsonMemberType> typename ElementHolder, template <typename ...> typename DestinationRange>
        using EnumerateView = Enumerate<Zipper<ElementHolder>::template Zipped, DestinationRange, MemberCount>;

        template <std::size_t SubViewBegin, std::size_t SubViewMemberCount>
        using NextViewSubView = View<SubViewBegin + Begin + MemberCount, SubViewMemberCount>;
    };

    using JsonStructure = typename MemberTypeSelector<JsonValueType::OBJECT, View<0, JsonEarlyStructure.children_count>>::MemberType;
};

template <FixedLengthString String>
constexpr auto operator"" _member()
{
    return CompileTimeValueHolder<FixedLengthString<String.string.size() - 1>(String.string.data())>{};
}

template <auto JsonEarlySturcture>
constexpr auto construct_json()
{
    return typename JsonStructureContext<JsonEarlySturcture>::JsonStructure{JsonEarlySturcture.members, 0, JsonEarlySturcture.children_count};
}

template <FixedLengthString String>
constexpr auto operator"" _json()
{
    using Context = ParseContext<String>;

    return construct_json<Context::parse_json()>();
}

template <typename Range>
constexpr void print_json(
    const Range &children,
    const Range &children_object_values)
{
    for (const auto &member : children)
    {
        if (member.type == JsonValueType::OBJECT)
        {
            std::cout << std::string_view{member.name} << " with value: {" << std::endl;

            print_json(std::ranges::subrange(children_object_values.begin() + member.object_start, member.member_count),
                       std::ranges::subrange(children_object_values.begin() + member.object_start + member.member_count, children_object_values.end()));

            std::cout << "}" << std::endl;
        }
        else if (member.type == JsonValueType::ARRAY)
        {
            std::cout << "List " << std::string_view{member.name} << " of length " << member.member_count << " with value: [" << std::endl;

            print_json(std::ranges::subrange(children_object_values.begin() + member.object_start, member.member_count),
                       std::ranges::subrange(children_object_values.begin() + member.object_start + member.member_count, children_object_values.end()));

            std::cout << "]" << std::endl;
        }
        else if (member.type == JsonValueType::NULL_VALUE)
        {
            std::cout << std::string_view{member.name} << " with null value." << std::endl;
        }
        else
        {
            std::cout << std::string_view{member.name} << " with value: <<";
            std::visit([](const auto &value) {
                if constexpr (!std::is_same_v<decltype(value), const std::monostate &>)
                    std::cout << value;
            },
                       member.get_value());
            std::cout << ">>" << std::endl;
        }
    }
}
