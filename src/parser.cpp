#define BOOST_SPIRIT_USE_PHOENIX_V3
#define BOOST_RESULT_OF_USE_DECLTYPE 1

#include <string>
#include <memory>
#include <exception>
#include <cstddef>
#include <utility>

#include <boost/format.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_as.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>

#include "parser.hpp"
#include "ast.hpp"

namespace dachs {
namespace syntax {

using std::size_t;

namespace detail {
    template<class Iterator>
    auto position_of(Iterator begin, Iterator current)
    {
        size_t line = 1;
        size_t col = 1;
        for (; begin != current; ++begin) {
            if (*begin == '\n') {
                col = 1;
                line++;
                continue;
            }
            col++;
        }
        return std::make_pair(line, col);
    }
} // namespace detail

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;
namespace ascii = boost::spirit::ascii;

using qi::_1;
using qi::_2;
using qi::_3;
using qi::_val;
using qi::lit;
using phx::bind;

// Helpers {{{
namespace detail {
    template<class Node>
    struct make_shared {
        template<class... Args>
        auto operator()(Args &&... args) const
        {
            return std::make_shared<Node>(std::forward<Args>(args)...);
        }
    };
} // namespace detail

template<class NodeType, class... Holders>
inline auto make_node_ptr(Holders &&... holders)
{
    return phx::bind(detail::make_shared<typename NodeType::element_type>{}, std::forward<Holders>(holders)...);
}
// }}}

template<class Iterator>
class grammar : public qi::grammar<Iterator, ast::node::program(), ascii::blank_type /*FIXME*/> {
    template<class Value, class... Extra>
    using rule = qi::rule<Iterator, Value, ascii::blank_type, Extra...>;

public:
    grammar() : grammar::base_type(program)
    {
        // FIXME: Temporary
        program
            = (
                postfix_expr > (qi::eol | qi::eoi)
            ) [
                _val = make_node_ptr<ast::node::program>(_1)
            ];

        literal
            = (
                  character_literal
                | string_literal
                | boolean_literal
                | float_literal
                | integer_literal
                | array_literal
                | tuple_literal
            ) [
                _val = make_node_ptr<ast::node::literal>(_1)
            ];

        character_literal
            = (
                '\'' > qi::char_ > '\''
            ) [
                _val = make_node_ptr<ast::node::character_literal>(_1)
            ];

        float_literal
            = (
                (qi::real_parser<double, qi::strict_real_policies<double>>())
            ) [
                _val = make_node_ptr<ast::node::float_literal>(_1)
            ];

        boolean_literal
            = (
                qi::bool_
            ) [
                _val = make_node_ptr<ast::node::boolean_literal>(_1)
            ];

        string_literal
            = (
                qi::as_string[qi::lexeme['"' > *(qi::char_ - '"') > '"']]
            ) [
                _val = make_node_ptr<ast::node::string_literal>(_1)
            ];

        integer_literal
            = (
                  (qi::lexeme[qi::uint_ >> 'u']) | qi::int_
            ) [
                _val = make_node_ptr<ast::node::integer_literal>(_1)
            ];

        // FIXME: Temporary
        array_literal
            = (
                lit('[') >> ']'
            ) [
                _val = make_node_ptr<ast::node::array_literal>()
            ];

        // FIXME: Temporary
        tuple_literal
            = (
                '(' >> +lit(',') >> ')'
            ) [
                _val = make_node_ptr<ast::node::tuple_literal>()
            ];

        identifier
            = (
                qi::as_string[(qi::alpha | qi::char_('_')) >> *(qi::alnum | qi::char_('_'))]
            ) [
                _val = make_node_ptr<ast::node::identifier>(_1)
            ];

        primary_expr
            = (
                literal | identifier // | '(' >> expr >> ')'
            ) [
                _val = make_node_ptr<ast::node::primary_expr>(_1)
            ];

        index_access
            = (
                lit('[') >> /* -expression >> */ ']' // TODO: Temporary
            ) [
                _val = make_node_ptr<ast::node::index_access>()
            ];

        member_access
            = (
                '.' >> identifier
            ) [
                _val = make_node_ptr<ast::node::member_access>(_1)
            ];

        function_call
            = (
                '(' >> qi::eps >> ')' // TODO: Temporary
            ) [
                _val = make_node_ptr<ast::node::function_call>()
            ];

        postfix_expr
            = (
                  primary_expr >> *(member_access | index_access | function_call)
            ) [
                _val = make_node_ptr<ast::node::postfix_expr>(_1, _2)
            ];

        qi::on_error<qi::fail>
        (
            program,
            // qi::_1 : begin of string to parse
            // qi::_2 : end of string to parse
            // qi::_3 : iterator at failed point
            // qi::_4 : what failed?
            std::cerr
                << phx::val("Syntax error at ")
                << bind([](auto const begin, auto const err_pos) -> std::string {
                        auto const pos = detail::position_of(begin, err_pos);
                        return (boost::format("line:%1%, col:%2%") % pos.first % pos.second).str();
                    }, _1, _3) << '\n'
                << "expected " << qi::_4 << '\n'
                << phx::val("here:\n\"")
                << phx::construct<std::string>(_3, _2)
                << phx::val("\"")
                << std::endl
        );
    }

    ~grammar()
    {}

private:
#define DACHS_PARSE_RULE(n) rule<ast::node::n()> n;

    DACHS_PARSE_RULE(program);
    DACHS_PARSE_RULE(literal);
    DACHS_PARSE_RULE(integer_literal);
    DACHS_PARSE_RULE(character_literal);
    DACHS_PARSE_RULE(float_literal);
    DACHS_PARSE_RULE(boolean_literal);
    DACHS_PARSE_RULE(string_literal);
    DACHS_PARSE_RULE(array_literal);
    DACHS_PARSE_RULE(tuple_literal);
    DACHS_PARSE_RULE(identifier);
    DACHS_PARSE_RULE(primary_expr);
    DACHS_PARSE_RULE(index_access);
    DACHS_PARSE_RULE(member_access);
    DACHS_PARSE_RULE(function_call);
    DACHS_PARSE_RULE(postfix_expr);

#undef DACHS_PARSE_RULE
};


ast::ast parser::parse(std::string const& code)
{
    auto itr = std::begin(code);
    auto const end = std::end(code);
    grammar<decltype(itr)> spiritual_parser;
    ast::node::program root;

    if (!qi::phrase_parse(itr, end, spiritual_parser, ascii::blank, root) || itr != end) {
        throw parse_error{detail::position_of(std::begin(code), itr)};
    }

    return {root};
}

} // namespace syntax
} // namespace dachs
