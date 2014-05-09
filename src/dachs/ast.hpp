#if !defined DACHS_AST_HPP_INCLUDED
#define      DACHS_AST_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <type_traits>
#include <cstddef>
#include <boost/variant/variant.hpp>
#include <boost/optional.hpp>

#include "dachs/ast_fwd.hpp"
#include "dachs/scope_fwd.hpp"
#include "dachs/symbol.hpp"
#include "dachs/type.hpp"


// Note:
// This AST is heterogeneous (patially homogeneous).
// Homogeneous AST with boost::variant is too hard because of high compile-time cost.
//
//     boost::variant<Node1, Node2, ..., NodeX> node;
//
// I now consider about type erasure technique to deal with all heterogeneous nodes
// as the same node like below.  This requires the list of all nodes at 'FIXME:' points.
// But I can reduce to one list using preprocessor macro.  Though double dispatch technique
// resolve this all nodes enumeration, it requires virtual function implementation in all nodes.
//
//
// #include <memory>
//
// template<class T, class... Args>
// inline
// std::unique_ptr<T> make_unique(Args &&... args)
// {
//     return std::unique_ptr<T>{new T{std::forward<Args>(args)...}};
// }
//
// // Forward declaration for test nodes
// struct ast_node_1;
// struct ast_node_2;
//
// class visitor {
//
//     struct visitor_holder_base {
//
//         virtual void visit(ast_node_1 const& lit) const = 0;
//         virtual void visit(ast_node_1 &lit) = 0;
//         virtual void visit(ast_node_2 const& lit) const = 0;
//         virtual void visit(ast_node_2 &lit) = 0;
//         // ...
//         // FIXME: So many pure virtual functions
//
//         virtual ~visitor_holder_base()
//         {}
//     };
//
//     template<class Visitor>
//     struct visitor_holder : visitor_holder_base {
//         Visitor &v;
//
//         void visit(ast_node_1 const& lit) const override { v(lit); }
//         void visit(ast_node_1 &lit) override { v(lit); }
//         void visit(ast_node_2 const& lit) const override { v(lit); }
//         void visit(ast_node_2 &lit) override { v(lit); }
//         // ...
//         // FIXME: So many virtual functions
//
//         visitor_holder(Visitor &v)
//             : v(v)
//         {}
//
//         virtual ~visitor_holder()
//         {}
//     };
//
//     std::unique_ptr<visitor_holder_base> holder;
//
// public:
//
//     template<class AnyVisitor>
//     explicit visitor(AnyVisitor &v)
//         : holder(make_unique<visitor_holder<AnyVisitor>>(v))
//     {}
//
//     visitor(visitor const&) = delete;
//     visitor &operator=(visitor const&) = delete;
//
//     template<class T>
//     void visit(T const& node) const
//     {
//         holder->visit(node);
//     }
//
//     template<class T>
//     void visit(T &node)
//     {
//         holder->visit(node);
//     }
// };
//
// class node {
//
//     struct node_holder_base {
//
//         // Interfaces here
//         virtual void apply(visitor const& v) const = 0;
//         virtual void apply(visitor &v) = 0;
//
//         virtual ~node_holder_base()
//         {}
//     };
//
//     template<class Node>
//     struct node_holder : node_holder_base {
//         Node node;
//
//         node_holder(Node const& node)
//             : node(node)
//         {}
//
//         node_holder(Node && node)
//             : node(node)
//         {}
//
//         void apply(visitor const& v) const override
//         {
//             v.visit(node);
//         }
//
//         void apply(visitor &v) override
//         {
//             v.visit(node);
//         }
//
//         virtual ~node_holder()
//         {}
//     };
//
//     std::shared_ptr<node_holder_base> holder;
//
// public:
//
//     template<class AnyNode>
//     node(AnyNode const& n)
//         : holder(std::make_shared<node_holder<AnyNode>>(n))
//     {}
//
//     void apply(visitor const& v)
//     {
//         holder->apply(v);
//     }
//
//     template<class V>
//     void apply(V const& v) const
//     {
//         holder->apply(visitor{v});
//     }
//
//     template<class V>
//     void apply(V &v)
//     {
//         holder->apply(visitor{v});
//     }
// };
//
// #include <iostream>
//
// // Test nodes
// struct ast_node_1 {
//     char value;
// };
// struct ast_node_2 {
//     float value;
// };
//
// // Test visitor
// struct printer {
//     template<class T>
//     void operator()(T const& t) const
//     {
//         std::cout << t.value << std::endl;
//     }
// };
//
// int main()
// {
//     node n1 = ast_node_1{'c'};
//     node n2 = ast_node_2{3.14};
//
//     printer p;
//     n1.apply(p);
//     n2.apply(p);
//
//     return 0;
// }
//


namespace dachs {
namespace ast {

namespace node_type {

struct expression : public base {
    type::type type;

    expression() = default;

    template<class Type>
    explicit expression(Type && t)
        : type{std::forward<Type>(t)}
    {}

    virtual ~expression() noexcept
    {}
};

struct statement : public base {
    virtual ~statement() noexcept
    {}
};

struct primary_literal final : public expression {
    boost::variant< char
                  , double
                  , bool
                  , std::string
                  , int
                  , unsigned int
                > value;

    template<class T>
    explicit primary_literal(T && v) noexcept
        : value{std::forward<T>(v)}
    {}

    std::string to_string() const noexcept override;
};

struct symbol_literal final : public expression {
    std::string value;

    explicit symbol_literal(std::string const& s) noexcept
        : expression(), value(s)
    {}

    std::string to_string() const noexcept override
    {
        return "SYMBOL_LITERAL: " + value;
    }
};

struct array_literal final : public expression {
    std::vector<node::compound_expr> element_exprs;

    explicit array_literal(std::vector<node::compound_expr> const& elems) noexcept
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const noexcept override
    {
        return "ARRAY_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct tuple_literal final : public expression {
    std::vector<node::compound_expr> element_exprs;

    explicit tuple_literal(decltype(element_exprs) const& elems) noexcept
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const noexcept override
    {
        return "TUPLE_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct dict_literal final : public expression {
    using dict_elem_type =
        std::pair<node::compound_expr, node::compound_expr>;
    using value_type =
        std::vector<dict_elem_type>;

    value_type value;

    explicit dict_literal(value_type const& m) noexcept
        : value(m)
    {}

    std::string to_string() const noexcept override
    {
        return "DICT_LITERAL: size=" + std::to_string(value.size());
    }
};

// This node will have kind of variable (global, member, local variables and functions)
struct var_ref final : public expression {
    std::string name;
    dachs::symbol::weak_var_symbol symbol;

    explicit var_ref(std::string const& n) noexcept
        : expression(), name(n)
    {}

    std::string to_string() const noexcept override
    {
        return "VAR_REFERENCE: " + name;
    }
};

struct parameter final : public base {
    bool is_var;
    std::string name;
    boost::optional<node::qualified_type> param_type;
    dachs::symbol::weak_var_symbol param_symbol;
    boost::optional<dachs::symbol::template_type_symbol> template_type_ref;

    parameter(bool const v, std::string const& n, decltype(param_type) const& t) noexcept
        : base(), is_var(v), name(n), param_type(t)
    {}

    std::string to_string() const noexcept override
    {
        return "PARAMETER: " + name + " (" + (is_var ? "mutable)" : "immutable)");
    }
};

struct function_call final : public expression {
    std::vector<node::compound_expr> args;

    function_call(decltype(args) const& args) noexcept
        : expression(), args(args)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNCTION_CALL";
    }
};

struct object_construct final : public expression {
    node::qualified_type obj_type;
    std::vector<node::compound_expr> args;

    object_construct(node::qualified_type const& t,
                     decltype(args) const& args) noexcept
        : expression(), obj_type(t), args(args)
    {}

    std::string to_string() const noexcept override
    {
        return "OBJECT_CONSTRUCT";
    }
};

struct index_access final : public expression {
    node::compound_expr index_expr;

    index_access(node::compound_expr const& idx_expr) noexcept
        : expression(), index_expr(idx_expr)
    {}

    std::string to_string() const noexcept override
    {
        // return index_access ? "INDEX_ACCESS" : "INDEX_ACCESS : (no index)";
        return "INDEX_ACCESS : (no index)";
    }
};

struct member_access final : public expression {
    std::string member_name;

    explicit member_access(std::string const& member_name) noexcept
        : expression(), member_name(member_name)
    {}

    std::string to_string() const noexcept override
    {
        return "MEMBER_ACCESS: " + member_name;
    }
};

struct postfix_expr final : public expression {
    // Note: add do-end block
    node::primary_expr prefix;
    using postfix_type =
        boost::variant< node::member_access
                      , node::index_access
                      , node::function_call >;
    std::vector<postfix_type> postfixes;

    postfix_expr(node::primary_expr const& pe, std::vector<postfix_type> const& ps) noexcept
        : expression(), prefix(pe), postfixes(ps)
    {}

    std::string to_string() const noexcept override
    {
        return "POSTFIX_EXPR";
    }
};

struct unary_expr final : public expression {
    std::vector<std::string> values;
    node::postfix_expr expr;

    unary_expr(std::vector<std::string> const& ops, node::postfix_expr const& expr) noexcept
        : expression(), values(ops), expr(expr)
    {}

    std::string to_string() const noexcept override;
};

struct primary_type final : public base {
    std::string template_name;
    boost::optional<std::vector<node::qualified_type>> instantiated_templates;

    primary_type(std::string const& tmpl
                , decltype(instantiated_templates) const& instantiated) noexcept
        : base(), template_name(tmpl), instantiated_templates(instantiated)
    {}

    bool is_template() const noexcept
    {
        return instantiated_templates;
    }

    std::string to_string() const noexcept override
    {
        return "PRIMARY_TYPE: " + template_name + " (" + (instantiated_templates ? "template)" : "not template)");
    }
};

struct array_type final : public base {
    node::qualified_type elem_type;

    explicit array_type(node::qualified_type const& elem) noexcept
        : elem_type(elem)
    {}

    std::string to_string() const noexcept override
    {
        return "ARRAY_TYPE";
    }
};

struct dict_type final : public base {
    node::qualified_type key_type;
    node::qualified_type value_type;

    dict_type(node::qualified_type const& key_type,
             node::qualified_type const& value_type) noexcept
        : key_type(key_type), value_type(value_type)
    {}

    std::string to_string() const noexcept override
    {
        return "DICT_TYPE";
    }
};

struct tuple_type final : public base {
    std::vector<node::qualified_type> arg_types; // Note: length of this variable should not be 1

    explicit tuple_type(std::vector<node::qualified_type> const& args) noexcept
        : arg_types(args)
    {}

    std::string to_string() const noexcept override
    {
        return "TUPLE_TYPE";
    }
};

struct func_type final : public base {
    std::vector<node::qualified_type> arg_types;
    node::qualified_type ret_type;

    func_type(decltype(arg_types) const& arg_t
            , node::qualified_type const& ret_t) noexcept
        : base(), arg_types(arg_t), ret_type(ret_t)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNC_TYPE";
    }
};

struct proc_type final : public base {
    std::vector<node::qualified_type> arg_types;

    explicit proc_type(decltype(arg_types) const& arg_t) noexcept
        : base(), arg_types(arg_t)
    {}

    std::string to_string() const noexcept override
    {
        return "PROC_TYPE";
    }
};

struct qualified_type final : public base {
    boost::optional<symbol::qualifier> value;
    node::compound_type type;

    qualified_type(decltype(value) const& q,
                   node::compound_type const& t) noexcept
        : value(q), type(t)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"QUALIFIED_TYPE: "} + (value ? symbol::to_string(*value) : "not qualified");
    }
};

struct cast_expr final : public expression {
    std::vector<node::qualified_type> dest_types;
    node::unary_expr source_expr;

    cast_expr(decltype(dest_types) const& types,
              node::unary_expr const& expr) noexcept
        : expression(), dest_types(types), source_expr(expr)
    {}

    std::string to_string() const noexcept override
    {
        return "CAST_EXPR";
    }
};

template<class FactorType>
struct multi_binary_expr : public expression {
    using rhs_type
        = std::pair<std::string, FactorType>;
    using rhss_type
        = std::vector<rhs_type>;

    FactorType lhs;
    rhss_type rhss;

    multi_binary_expr(FactorType const& lhs, rhss_type const& rhss) noexcept
        : expression(), lhs(lhs), rhss(rhss)
    {}

    virtual ~multi_binary_expr()
    {}
};

struct mult_expr final : public multi_binary_expr<node::cast_expr> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "MULT_EXPR";
    }
};

struct additive_expr final : public multi_binary_expr<node::mult_expr> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "ADDITIVE_EXPR";
    }
};

struct shift_expr final : public multi_binary_expr<node::additive_expr> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "SHIFT_EXPR";
    }
};

struct relational_expr final : public multi_binary_expr<node::shift_expr> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "RELATIONAL_EXPR";
    }
};

struct equality_expr final : public multi_binary_expr<node::relational_expr> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "EQUALITY_EXPR";
    }
};

template<class FactorType>
struct binary_expr : public expression {
    FactorType lhs;
    std::vector<FactorType> rhss;

    binary_expr(FactorType const& lhs, decltype(rhss) const& rhss) noexcept
        : expression(), lhs(lhs), rhss(rhss)
    {}

    virtual ~binary_expr()
    {}
};

struct and_expr final : public binary_expr<node::equality_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "AND_EXPR";
    }
};

struct xor_expr final : public binary_expr<node::and_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "XOR_EXPR";
    }
};

struct or_expr final : public binary_expr<node::xor_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "OR_EXPR";
    }
};

struct logical_and_expr final : public binary_expr<node::or_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "LOGICAL_AND_EXPR";
    }
};

struct logical_or_expr final : public binary_expr<node::logical_and_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "LOGICAL_OR_EXPR";
    }
};

struct if_expr final : public expression {
    symbol::if_kind kind;
    node::compound_expr condition_expr, then_expr, else_expr;

    if_expr(symbol::if_kind const kind,
            node::compound_expr const& condition,
            node::compound_expr const& then,
            node::compound_expr const& else_) noexcept
        : expression(), kind(kind), condition_expr(condition), then_expr(then), else_expr(else_)
    {}

    std::string to_string() const noexcept override
    {
        return "IF_EXPR: " + symbol::to_string(kind);
    }
};

struct range_expr final : public expression {
    using rhs_type
        = std::pair<
            std::string,
            node::logical_or_expr
        >;
    node::logical_or_expr lhs;
    boost::optional<rhs_type> maybe_rhs;

    range_expr(node::logical_or_expr const& lhs,
               decltype(maybe_rhs) const& maybe_rhs) noexcept
        : lhs(lhs), maybe_rhs(maybe_rhs)
    {}

    bool has_range() const noexcept
    {
        return maybe_rhs;
    }

    std::string to_string() const noexcept override
    {
        return "RANGE_EXPR: " + (maybe_rhs ? (*maybe_rhs).first : "no range");
    }
};

struct compound_expr final : public expression {
    using expr_type
        = boost::variant<node::range_expr,
                         node::if_expr>;
    expr_type child_expr;
    boost::optional<node::qualified_type> maybe_type;

    template<class T>
    compound_expr(T const& e, decltype(maybe_type) const& t) noexcept
        : expression(), child_expr(e), maybe_type(t)
    {}

    std::string to_string() const noexcept override
    {
        return "COMPOUND_EXPR";
    }
};

struct variable_decl final : public base {
    bool is_var;
    std::string name;
    boost::optional<node::qualified_type> maybe_type;
    dachs::symbol::weak_var_symbol symbol;

    variable_decl(bool const var,
                  std::string const& name,
                  decltype(maybe_type) const& type) noexcept
        : is_var(var), name(name), maybe_type(type)
    {}

    std::string to_string() const noexcept override
    {
        return "VARIABLE_DECL: " + name + " (" + (is_var ? "mutable)" : "immutable)");
    }
};

struct initialize_stmt final : public statement {
    std::vector<node::variable_decl> var_decls;
    boost::optional<std::vector<node::compound_expr>> maybe_rhs_exprs;

    initialize_stmt(decltype(var_decls) const& vars,
                    decltype(maybe_rhs_exprs) const& rhss) noexcept
        : statement(), var_decls(vars), maybe_rhs_exprs(rhss)
    {}

    std::string to_string() const noexcept override
    {
        return "INITIALIZE_STMT";
    }
};

struct assignment_stmt final : public statement {
    std::vector<node::postfix_expr> assignees;
    symbol::assign_operator assign_op;
    std::vector<node::compound_expr> rhs_exprs;

    assignment_stmt(decltype(assignees) const& assignees,
                    symbol::assign_operator assign_op,
                    decltype(rhs_exprs) const& rhs_exprs) noexcept
        : statement(), assignees(assignees), assign_op(assign_op), rhs_exprs(rhs_exprs)
    {}

    std::string to_string() const noexcept override
    {
        return "ASSIGNMENT_STMT";
    }
};

struct if_stmt final : public statement {
    using elseif_type
        = std::pair<node::compound_expr, node::statement_block>;

    symbol::if_kind kind;
    node::compound_expr condition;
    node::statement_block then_stmts;
    std::vector<elseif_type> elseif_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;

    if_stmt(symbol::if_kind const kind,
            node::compound_expr const& cond,
            node::statement_block const& then,
            decltype(elseif_stmts_list) const& elseifs,
            decltype(maybe_else_stmts) const& maybe_else) noexcept
        : statement(), kind(kind), condition(cond), then_stmts(then), elseif_stmts_list(elseifs), maybe_else_stmts(maybe_else)
    {}

    std::string to_string() const noexcept override
    {
        return "IF_STMT: " + symbol::to_string(kind);
    }
};

struct return_stmt final : public statement {
    std::vector<node::compound_expr> ret_exprs;

    explicit return_stmt(std::vector<node::compound_expr> const& rets) noexcept
        : statement(), ret_exprs(rets)
    {}

    std::string to_string() const noexcept override
    {
        return "RETURN_STMT";
    }
};

struct case_stmt final : public statement {
    using when_type
        = std::pair<node::compound_expr, node::statement_block>;

    std::vector<when_type> when_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;

    case_stmt(decltype(when_stmts_list) const& whens,
              decltype(maybe_else_stmts) const& elses) noexcept
        : statement(), when_stmts_list(whens), maybe_else_stmts(elses)
    {}

    std::string to_string() const noexcept override
    {
        return "CASE_STMT";
    }
};

struct switch_stmt final : public statement {
    using when_type
        = std::pair<
            std::vector<node::compound_expr>,
            node::statement_block
        >;

    node::compound_expr target_expr;
    std::vector<when_type> when_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;

    switch_stmt(node::compound_expr const& target,
                decltype(when_stmts_list) const& whens,
                decltype(maybe_else_stmts) const& elses) noexcept
        : statement(), target_expr(target), when_stmts_list(whens), maybe_else_stmts(elses)
    {}

    std::string to_string() const noexcept override
    {
        return "SWITCH_STMT";
    }
};

struct for_stmt final : public statement {
    std::vector<node::parameter> iter_vars;
    node::compound_expr range_expr;
    node::statement_block body_stmts;

    for_stmt(decltype(iter_vars) const& iters,
             node::compound_expr const& range,
             node::statement_block body) noexcept
        : statement(), iter_vars(iters), range_expr(range), body_stmts(body)
    {}

    std::string to_string() const noexcept override
    {
        return "FOR_STMT";
    }
};

struct while_stmt final : public statement {
    node::compound_expr condition;
    node::statement_block body_stmts;

    while_stmt(node::compound_expr const& cond,
               node::statement_block const& body) noexcept
        : statement(), condition(cond), body_stmts(body)
    {}

    std::string to_string() const noexcept override
    {
        return "WHILE_STMT";
    }
};

struct postfix_if_stmt final : public statement {
    using body_type
        = boost::variant<
            node::assignment_stmt
          , node::return_stmt
          , node::compound_expr
        >;
    body_type body;
    symbol::if_kind kind;
    node::compound_expr condition;

    template<class T>
    postfix_if_stmt(T const& body,
                    symbol::if_kind const kind,
                    node::compound_expr const& cond) noexcept
        : statement(), body(body), kind(kind), condition(cond)
    {}

    std::string to_string() const noexcept override
    {
        return "POSTFIX_IF_STMT: " + symbol::to_string(kind);
    }
};

struct statement_block final : public base {
    using block_type
        = std::vector<node::compound_stmt>;

    block_type value;
    scope::weak_local_scope scope;

    explicit statement_block(block_type const& v) noexcept
        : value(v)
    {}

    explicit statement_block(boost::optional<block_type> const& ov) noexcept
        : value(ov ? *ov : block_type{})
    {}

    std::string to_string() const noexcept override
    {
        return "STATEMENT_BLOCK: size=" + std::to_string(value.size());
    }
};

struct function_definition final : public statement {
    symbol::func_kind kind;
    std::string name;
    std::vector<node::parameter> params;
    boost::optional<node::qualified_type> return_type;
    node::statement_block body;
    boost::optional<node::statement_block> ensure_body;
    scope::weak_func_scope scope;

    function_definition(symbol::func_kind const k
                      , std::string const& n
                      , decltype(params) const& p
                      , decltype(return_type) const& ret
                      , node::statement_block const& block
                      , decltype(ensure_body) const& ensure) noexcept
        : statement(), kind(k), name(n), params(p), return_type(ret), body(block), ensure_body(ensure)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNC_DEFINITION: " + symbol::to_string(kind) + ' ' + name;
    }
};

struct constant_decl final : public base {
    std::string name;
    boost::optional<node::qualified_type> maybe_type;
    dachs::symbol::weak_var_symbol symbol;

    constant_decl(std::string const& name,
                  decltype(maybe_type) const& type) noexcept
        : base(), name(name), maybe_type(type)
    {}

    std::string to_string() const noexcept override
    {
        return "CONSTANT_DECL: " + name;
    }
};

struct constant_definition final : public statement {
    std::vector<node::constant_decl> const_decls;
    std::vector<node::compound_expr> initializers;

    constant_definition(decltype(const_decls) const& const_decls
                      , decltype(initializers) const& initializers) noexcept
        : statement(), const_decls(const_decls), initializers(initializers)
    {}

    std::string to_string() const noexcept override
    {
        return "CONSTANT_DEFINITION";
    }
};

struct program final : public base {
    std::vector<node::global_definition> inu;

    explicit program(decltype(inu) const& value) noexcept
        : base(), inu(value)
    {}

    std::string to_string() const noexcept override
    {
        return "PROGRAM: num of definitions is " + std::to_string(inu.size());
    }
};

} // namespace node_type

namespace traits {

template<class T>
struct is_node : std::is_base_of<node_type::base, typename std::remove_cv<T>::type> {};

template<class T>
struct is_expression : std::is_base_of<node_type::expression, typename std::remove_cv<T>::type> {};

template<class T>
struct is_statement : std::is_base_of<node_type::statement, typename std::remove_cv<T>::type> {};

} // namespace traits

struct ast {
    node::program root;
};

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_HPP_INCLUDED
