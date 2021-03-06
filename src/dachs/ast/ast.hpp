#if !defined DACHS_AST_AST_HPP_INCLUDED
#define      DACHS_AST_AST_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <cstddef>
#include <cassert>
#include <boost/variant/variant.hpp>
#include <boost/optional.hpp>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/helper/variant.hpp"

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
    std::vector<node::any_expr> element_exprs;

    explicit array_literal(std::vector<node::any_expr> const& elems) noexcept
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const noexcept override
    {
        return "ARRAY_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct tuple_literal final : public expression {
    std::vector<node::any_expr> element_exprs;

    explicit tuple_literal(decltype(element_exprs) const& elems) noexcept
        : expression(), element_exprs(elems)
    {}

    tuple_literal() = default;

    std::string to_string() const noexcept override
    {
        return "TUPLE_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct dict_literal final : public expression {
    using dict_elem_type =
        std::pair<node::any_expr, node::any_expr>;
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
    bool is_lhs_of_assignment = false;

    explicit var_ref(std::string const& n) noexcept
        : expression(), name(n)
    {}

    bool is_ignored_var() const noexcept
    {
        return name == "_" && symbol.expired();
    }

    std::string to_string() const noexcept override
    {
        return "VAR_REFERENCE: " + name;
    }
};

struct parameter final : public base {
    bool is_var;
    std::string name;
    boost::optional<node::any_type> param_type;
    dachs::symbol::weak_var_symbol param_symbol;
    type::type type;

    template<class T>
    parameter(T const& v, std::string const& n, decltype(param_type) const& t) noexcept
        : base(), is_var(v), name(n), param_type(t)
    {}

    std::string to_string() const noexcept override
    {
        return "PARAMETER: " + name + " (" + (is_var ? "mutable)" : "immutable)");
    }
};

struct func_invocation final : public expression {
    node::any_expr child;
    std::vector<node::any_expr> args;
    bool is_monad_invocation = false;
    scope::weak_func_scope callee_scope;
    boost::optional<node::function_definition> do_block;

    func_invocation(
            node::any_expr const& c,
            std::vector<node::any_expr> const& args,
            decltype(do_block) const f = boost::none
        ) noexcept
        : expression(), child(c), args(args), do_block(std::move(f))
    {}

    // Note: For UFCS
    func_invocation(
            node::any_expr const& c,
            node::any_expr const& head,
            std::vector<node::any_expr> const& tail,
            decltype(do_block) const f = nullptr
        ) noexcept
        : expression(), child(c), args({head}), do_block(std::move(f))
    {
        args.insert(args.end(), tail.begin(), tail.end());
    }

    std::string to_string() const noexcept override
    {
        return "FUNC_INVOCATION";
    }
};

struct object_construct final : public expression {
    node::any_type obj_type;
    std::vector<node::any_expr> args;

    object_construct(node::any_type const& t,
                     decltype(args) const& args) noexcept
        : expression(), obj_type(t), args(args)
    {}

    std::string to_string() const noexcept override
    {
        return "OBJECT_CONSTRUCT";
    }
};

struct index_access final : public expression {
    node::any_expr child, index_expr;

    index_access(node::any_expr const& c, node::any_expr const& idx_expr) noexcept
        : expression(), child(c), index_expr(idx_expr)
    {}

    std::string to_string() const noexcept override
    {
        return "INDEX_ACCESS";
    }
};

struct ufcs_invocation final : public expression {
    node::any_expr child;
    std::string member_name;
    scope::weak_func_scope callee_scope;
    boost::optional<node::function_definition> do_block;
    boost::optional<node::lambda_expr> do_block_object;

    ufcs_invocation(
            node::any_expr const& c,
            std::string const& member_name,
            decltype(do_block) const f = boost::none
        ) noexcept
        : expression(), child(c), member_name(member_name), do_block(std::move(f))
    {}

    std::string to_string() const noexcept override
    {
        return "UFCS_INVOCATION: " + member_name;
    }
};

struct unary_expr final : public expression {
    std::string op;
    node::any_expr expr;

    unary_expr(std::string const& op, node::any_expr const& expr) noexcept
        : expression(), op(op), expr(expr)
    {}

    std::string to_string() const noexcept override
    {
        return "UNARY_EXPR: " + op;
    }
};

struct cast_expr final : public expression {
    node::any_expr child;
    node::any_type casted_type;

    cast_expr(node::any_expr const& c,
              node::any_type const& t) noexcept
        : expression(), child(c), casted_type(t)
    {}

    std::string to_string() const noexcept override
    {
        return "CAST_EXPR";
    }
};

struct binary_expr final : public expression {
    node::any_expr lhs, rhs;
    std::string op;

    binary_expr(node::any_expr const& lhs
                    , std::string const& op
                    , node::any_expr const& rhs) noexcept
        : expression(), lhs(lhs), rhs(rhs), op(op)
    {}

    std::string to_string() const noexcept override
    {
        return "BINARY_EXPR: " + op;
    }
};

struct if_expr final : public expression {
    symbol::if_kind kind;
    node::any_expr condition_expr, then_expr, else_expr;

    if_expr(symbol::if_kind const kind,
            node::any_expr const& condition,
            node::any_expr const& then,
            node::any_expr const& else_) noexcept
        : expression(), kind(kind), condition_expr(condition), then_expr(then), else_expr(else_)
    {}

    std::string to_string() const noexcept override
    {
        return "IF_EXPR: " + symbol::to_string(kind);
    }
};

struct typed_expr final : public expression {
    node::any_expr child_expr;
    node::any_type specified_type;

    typed_expr(node::any_expr const& e, node::any_type const& t) noexcept
        : expression(), child_expr(e), specified_type(t)
    {}

    std::string to_string() const noexcept override
    {
        return "TYPED_EXPR";
    }
};

struct primary_type final : public base {
    std::string template_name;
    std::vector<node::any_type> instantiated_templates;

    primary_type(std::string const& tmpl
                , decltype(instantiated_templates) const& instantiated) noexcept
        : base(), template_name(tmpl), instantiated_templates(instantiated)
    {}

    bool is_template() const noexcept
    {
        return !instantiated_templates.empty();
    }

    std::string to_string() const noexcept override
    {
        return "PRIMARY_TYPE: " + template_name + " (" + (is_template() ? "template)" : "not template)");
    }
};

struct array_type final : public base {
    node::any_type elem_type;

    explicit array_type(node::any_type const& elem) noexcept
        : elem_type(elem)
    {}

    std::string to_string() const noexcept override
    {
        return "ARRAY_TYPE";
    }
};

struct dict_type final : public base {
    node::any_type key_type;
    node::any_type value_type;

    dict_type(node::any_type const& key_type,
             node::any_type const& value_type) noexcept
        : key_type(key_type), value_type(value_type)
    {}

    std::string to_string() const noexcept override
    {
        return "DICT_TYPE";
    }
};

struct tuple_type final : public base {
    // Note: length of this variable should not be 1
    std::vector<node::any_type> arg_types;

    tuple_type() = default;

    explicit tuple_type(std::vector<node::any_type> const& args) noexcept
        : arg_types(args)
    {}

    std::string to_string() const noexcept override
    {
        return "TUPLE_TYPE";
    }
};

struct func_type final : public base {
    std::vector<node::any_type> arg_types;
    boost::optional<node::any_type> ret_type = boost::none;

    func_type(decltype(arg_types) const& arg_t
            , node::any_type const& ret_t) noexcept
        : base(), arg_types(arg_t), ret_type(ret_t)
    {}

    explicit func_type(decltype(arg_types) const& arg_t) noexcept
        : base(), arg_types(arg_t)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"FUNC_TYPE: "} + (ret_type ? "func" : "proc");
    }
};

struct qualified_type final : public base {
    symbol::qualifier qualifier;
    node::any_type type;

    qualified_type(symbol::qualifier const& q,
                   node::any_type const& t) noexcept
        : qualifier(q), type(t)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"any_type: "} + symbol::to_string(qualifier);
    }
};

struct variable_decl final : public base {
    bool is_var;
    std::string name;
    boost::optional<node::any_type> maybe_type;
    dachs::symbol::weak_var_symbol symbol;

    template<class T>
    variable_decl(T const& var,
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
    boost::optional<std::vector<node::any_expr>> maybe_rhs_exprs;

    initialize_stmt(decltype(var_decls) const& vars,
                    decltype(maybe_rhs_exprs) const& rhss = boost::none) noexcept
        : statement(), var_decls(vars), maybe_rhs_exprs(rhss)
    {}

    std::string to_string() const noexcept override
    {
        return "INITIALIZE_STMT";
    }
};

struct assignment_stmt final : public statement {
    std::vector<node::any_expr> assignees;
    std::string op;
    std::vector<node::any_expr> rhs_exprs;

    assignment_stmt(decltype(assignees) const& assignees,
                    std::string const& op,
                    decltype(rhs_exprs) const& rhs_exprs) noexcept
        : statement(), assignees(assignees), op(op), rhs_exprs(rhs_exprs)
    {}

    std::string to_string() const noexcept override
    {
        return "ASSIGNMENT_STMT";
    }
};

struct if_stmt final : public statement {
    using elseif_type
        = std::pair<node::any_expr, node::statement_block>;

    symbol::if_kind kind;
    node::any_expr condition;
    node::statement_block then_stmts;
    std::vector<elseif_type> elseif_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;

    if_stmt(symbol::if_kind const kind,
            node::any_expr const& cond,
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
    std::vector<node::any_expr> ret_exprs;
    type::type ret_type;

    explicit return_stmt(std::vector<node::any_expr> const& rets) noexcept
        : statement(), ret_exprs(rets)
    {}

    explicit return_stmt(node::any_expr const& ret) noexcept
        : statement(), ret_exprs({ret})
    {}

    std::string to_string() const noexcept override
    {
        return "RETURN_STMT";
    }
};

struct case_stmt final : public statement {
    using when_type
        = std::pair<node::any_expr, node::statement_block>;

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
            std::vector<node::any_expr>,
            node::statement_block
        >;

    node::any_expr target_expr;
    std::vector<when_type> when_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;

    switch_stmt(node::any_expr const& target,
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
    node::any_expr range_expr;
    node::statement_block body_stmts;

    for_stmt(decltype(iter_vars) const& iters,
             node::any_expr const& range,
             node::statement_block body) noexcept
        : statement(), iter_vars(iters), range_expr(range), body_stmts(body)
    {}

    std::string to_string() const noexcept override
    {
        return "FOR_STMT";
    }
};

struct while_stmt final : public statement {
    node::any_expr condition;
    node::statement_block body_stmts;

    while_stmt(node::any_expr const& cond,
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
          , node::any_expr
        >;
    body_type body;
    symbol::if_kind kind;
    node::any_expr condition;

    template<class T>
    postfix_if_stmt(T const& body,
                    symbol::if_kind const kind,
                    node::any_expr const& cond) noexcept
        : statement(), body(body), kind(kind), condition(cond)
    {}

    std::string to_string() const noexcept override
    {
        return "POSTFIX_IF_STMT: " + symbol::to_string(kind);
    }
};

struct let_stmt final : public statement {
    std::vector<node::initialize_stmt> inits;
    node::compound_stmt child_stmt;
    scope::weak_local_scope scope;

    let_stmt(decltype(inits) const& i, node::compound_stmt const& s)
        : statement(), inits(i), child_stmt(s)
    {
        assert(inits.size() > 0);
    }

    std::string to_string() const noexcept override
    {
        return "LET_STMT: " + std::to_string(inits.size()) + " inits";
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

    explicit statement_block(node::compound_stmt const& s) noexcept
        : value({s})
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
    boost::optional<node::any_type> return_type;
    node::statement_block body;
    boost::optional<node::statement_block> ensure_body;
    scope::weak_func_scope scope;
    boost::optional<type::type> ret_type;
    std::vector<node::function_definition> instantiated; // Note: This is not a part of AST!

    function_definition(symbol::func_kind const k
                      , std::string const& n
                      , decltype(params) const& p
                      , decltype(return_type) const& ret
                      , node::statement_block const& block
                      , decltype(ensure_body) const& ensure) noexcept
        : statement()
        , kind(k)
        , name(n)
        , params(p)
        , return_type(ret)
        , body(block)
        , ensure_body(ensure)
    {}

    function_definition(decltype(params) const& p, node::statement_block const& b) noexcept
        : statement()
        , kind(symbol::func_kind::func)
        , name("")
        , params(p)
        , return_type(boost::none)
        , body(b)
        , ensure_body(boost::none)
    {}

    bool is_template() const noexcept
    {
        for (auto const& p : params) {
            assert(p->type);
            if (p->type.is_template()) {
                return true;
            }
        }

        return false;
    }

    std::string to_string() const noexcept override
    {
        return "FUNC_DEFINITION: " + symbol::to_string(kind) + ' ' + name;
    }
};

struct lambda_expr final : public expression {
    node::function_definition def;

    explicit lambda_expr(decltype(def) const& d)
        : expression(), def(d)
    {}

    std::string to_string() const noexcept override
    {
        return "LAMBDA_EXPR: " + def->name;
    }
};

struct inu final : public base {
    std::vector<node::global_definition> definitions;

    explicit inu(decltype(definitions) const& defs) noexcept
        : base(), definitions(defs)
    {}

    std::string to_string() const noexcept override
    {
        return "PROGRAM: num of definitions is " + std::to_string(definitions.size());
    }
};

} // namespace node_type

struct ast {
    node::inu root;
    std::string const name;
};

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_AST_HPP_INCLUDED
