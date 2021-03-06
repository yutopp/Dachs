#include <cassert>
#include <cstddef>
#include <iterator>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/numeric.hpp>
#include <boost/algorithm/string/join.hpp>

#include "dachs/semantics/scope.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace scope_node {
namespace detail {

template<class FuncScope, class ArgTypes>
inline
std::size_t get_overloaded_function_score(FuncScope const& func, ArgTypes const& arg_types)
{
    if (arg_types.size() != func->params.size()) {
        return 0u;
    }

    std::size_t score = 1u;

    if (arg_types.size() == 0) {
        return score;
    }

    // Calculate the score of arguments' coincidence
    std::vector<std::size_t> v;
    v.reserve(arg_types.size());

    boost::transform(arg_types, func->params, std::back_inserter(v),
            [](auto const& arg_type, auto const& param)
            {
                assert(arg_type);
                if (type::is_a<type::template_type>(param->type)) {
                    // Function parameter is template.  It matches any types.
                    return 1u;
                }

                assert(param->type);

                if (param->type == arg_type) {
                    return 2u;
                } else {
                    return 0u;
                }
            });

    // Note:
    // If the function have no argument and return type is not specified, score is 1.
    return boost::accumulate(v, score, [](auto l, auto r){ return l*r; });
}

template<class Funcs, class Types>
boost::optional<scope::func_scope>
get_overloaded_function(Funcs const& candidates, std::string const& name, Types const& arg_types)
{
    boost::optional<scope::func_scope> result = boost::none;

    std::size_t score = 0u;
    for (auto const& f : candidates) {
        if (f->name != name) {
            continue;
        }

        auto const score_tmp = detail::get_overloaded_function_score(f, arg_types);
        if (score_tmp > score) {
            score = score_tmp;
            result = f;
        }
    }

    return result;
}

} // namespace detail

boost::optional<scope::func_scope>
global_scope::resolve_func(std::string const& name, std::vector<type::type> const& arg_types) const
{
    return detail::get_overloaded_function(functions, name, arg_types);
}

ast::node::function_definition func_scope::get_ast_node() const noexcept
{
    auto maybe_func_def = ast::node::get_shared_as<ast::node::function_definition>(ast_node);
    assert(maybe_func_def);
    return *maybe_func_def;
}

// Note:
// define_function() can't share the implementation with resolve_func()'s overload resolution because define_function() considers new function's template arguments
bool func_scope::operator==(func_scope const& rhs) const noexcept
{
    if (name != rhs.name || params.size() != rhs.params.size()) {
        return false;
    }

    // Check arguments' types
    for (auto const& t : helper::zipped(params, rhs.params)) {
        auto const& left_type = boost::get<0>(t)->type;
        auto const& right_type = boost::get<1>(t)->type;

        if (left_type.is_template() && right_type.is_template()) {
            // Both sides are template
            continue;
        } else if (!left_type.is_template() && !right_type.is_template()) {
            // Both sides are not template
            if (left_type != right_type) {
                return false;
            }
        } else {
            // One side is template and other side is not a template
            return false;
        }
    }

    // Note:
    // Reach here when arguments match completely

    return true;
}

std::string func_scope::to_string() const noexcept
{
    if (is_builtin) {
        return "func " + name + '('
            + boost::algorithm::join(
                    params |
                    boost::adaptors::transformed(
                        [](auto const& p)
                        {
                            return p->name;
                        }
                    ), ", "
                )
            + ')';
    }

    auto const def = get_ast_node();
    std::string ret = ast::symbol::to_string(def->kind) + ' ' + name + '(';

    ret += boost::algorithm::join(def->params | boost::adaptors::transformed([](auto const& p){ return p->type.to_string(); }), ", ");
    ret += ')';

    if (def->ret_type) {
        ret += ": " + def->ret_type->to_string();
    }

    return ret;
}

} // namespace scope_node

} // namespace dachs
