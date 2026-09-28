#include "gml_parser.h"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>

namespace khanary::world {

namespace {

// ── Lexer ─────────────────────────────────────────────────────────────────────

enum class TokKind { IDENT, INT, FLOAT, STRING, LBRACKET, RBRACKET, END };

struct Token {
    TokKind kind = TokKind::END;
    std::string str;
    int64_t ival = 0;
    double  fval = 0.0;
};

struct Lexer {
    const char* p;
    const char* end;

    explicit Lexer(const std::string& s) noexcept
        : p(s.data()), end(s.data() + s.size()) {}

    void skip_ws() noexcept {
        while (p < end && std::isspace(static_cast<unsigned char>(*p)))
            ++p;
    }

    Token next() {
        skip_ws();
        if (p >= end) return {};

        if (*p == '[') { ++p; return {TokKind::LBRACKET, "["}; }
        if (*p == ']') { ++p; return {TokKind::RBRACKET, "]"}; }

        if (*p == '"') {
            ++p;
            std::string s;
            while (p < end && *p != '"') {
                if (*p == '\\' && p + 1 < end) {
                    ++p;
                    switch (*p) {
                        case '"':  s += '"';  break;
                        case '\\': s += '\\'; break;
                        case 'n':  s += '\n'; break;
                        case 't':  s += '\t'; break;
                        default:   s += '\\'; s += *p; break;
                    }
                } else {
                    s += *p;
                }
                ++p;
            }
            if (p < end) ++p; // consume closing "
            return {TokKind::STRING, s};
        }

        // Number: optional sign then digits, optional fractional/exponent part
        const bool sign = (*p == '-' || *p == '+');
        if (sign && p + 1 < end && std::isdigit(static_cast<unsigned char>(*(p + 1)))) {
            return scan_number();
        }
        if (std::isdigit(static_cast<unsigned char>(*p))) {
            return scan_number();
        }

        // Identifier
        if (std::isalpha(static_cast<unsigned char>(*p)) || *p == '_') {
            const char* start = p;
            while (p < end &&
                   (std::isalnum(static_cast<unsigned char>(*p)) || *p == '_'))
                ++p;
            return {TokKind::IDENT, std::string(start, p)};
        }

        // Unknown character — skip and recurse
        ++p;
        return next();
    }

    Token peek() {
        const char* saved = p;
        Token t = next();
        p = saved;
        return t;
    }

private:
    Token scan_number() {
        const char* start = p;
        bool is_float = false;
        if (*p == '-' || *p == '+') ++p;
        while (p < end && std::isdigit(static_cast<unsigned char>(*p))) ++p;
        if (p < end && *p == '.') {
            is_float = true;
            ++p;
            while (p < end && std::isdigit(static_cast<unsigned char>(*p))) ++p;
        }
        if (p < end && (*p == 'e' || *p == 'E')) {
            is_float = true;
            ++p;
            if (p < end && (*p == '+' || *p == '-')) ++p;
            while (p < end && std::isdigit(static_cast<unsigned char>(*p))) ++p;
        }
        std::string ns(start, p);
        if (is_float) {
            Token t{TokKind::FLOAT, ns};
            t.fval = std::stod(ns);
            return t;
        }
        Token t{TokKind::INT, ns};
        t.ival = std::stoll(ns);
        return t;
    }
};

// ── Raw block tree ────────────────────────────────────────────────────────────

struct RawBlock {
    std::string key;
    bool is_block = false;
    int64_t ival  = 0;
    double  fval  = 0.0;
    std::string sval;
    std::vector<RawBlock> children;
};

std::vector<RawBlock> parse_body(Lexer& lex) {
    std::vector<RawBlock> items;
    while (true) {
        Token t = lex.peek();
        if (t.kind == TokKind::RBRACKET || t.kind == TokKind::END) {
            if (t.kind == TokKind::RBRACKET) lex.next();
            break;
        }
        if (t.kind != TokKind::IDENT) { lex.next(); continue; }

        RawBlock rb;
        rb.key = lex.next().str;

        Token val = lex.peek();
        if (val.kind == TokKind::LBRACKET) {
            lex.next();
            rb.is_block  = true;
            rb.children  = parse_body(lex);
        } else if (val.kind == TokKind::INT) {
            lex.next();
            rb.ival = val.ival;
        } else if (val.kind == TokKind::FLOAT) {
            lex.next();
            rb.fval = val.fval;
        } else if (val.kind == TokKind::STRING) {
            lex.next();
            rb.sval = val.str;
        }
        // If val is anything else (IDENT treated as key), leave defaults and
        // don't consume — the next iteration picks it up as a key.
        items.push_back(std::move(rb));
    }
    return items;
}

// ── Typed accessors ───────────────────────────────────────────────────────────

static std::optional<std::string>
get_str(const std::vector<RawBlock>& v, const std::string& k) {
    for (const auto& rb : v)
        if (rb.key == k && !rb.is_block) return rb.sval;
    return std::nullopt;
}

static std::optional<int64_t>
get_int(const std::vector<RawBlock>& v, const std::string& k) {
    for (const auto& rb : v)
        if (rb.key == k && !rb.is_block) return rb.ival;
    return std::nullopt;
}

static std::optional<double>
get_dbl(const std::vector<RawBlock>& v, const std::string& k) {
    for (const auto& rb : v) {
        if (rb.key == k && !rb.is_block) {
            return (rb.fval != 0.0) ? rb.fval : static_cast<double>(rb.ival);
        }
    }
    return std::nullopt;
}

// ── GML object builders ───────────────────────────────────────────────────────

static GmlNode build_node(const std::vector<RawBlock>& items) {
    GmlNode n;
    if (auto v = get_int(items, "id"))    n.id    = *v;
    if (auto v = get_str(items, "label")) n.label = *v;
    if (auto v = get_dbl(items, "x")) { n.x = *v; n.has_x = true; }
    if (auto v = get_dbl(items, "y")) { n.y = *v; n.has_y = true; }
    if (auto v = get_dbl(items, "z")) { n.z = *v; n.has_z = true; }
    if (auto v = get_dbl(items, "confidence")) n.confidence = *v;
    if (auto v = get_dbl(items, "static_weight")) n.static_weight = *v;
    if (auto v = get_dbl(items, "health")) n.health = *v;
    return n;
}

static GmlEdge build_edge(const std::vector<RawBlock>& items) {
    GmlEdge e;
    if (auto v = get_int(items, "source")) e.source = *v;
    if (auto v = get_int(items, "target")) e.target = *v;
    if (auto v = get_str(items, "label"))  e.label  = *v;
    if (auto v = get_str(items, "verb"))   e.verb   = *v;
    if (e.verb.empty()) {
        if (auto v = get_str(items, "relation")) e.verb = *v;
    }
    if (e.label.empty() && !e.verb.empty()) e.label = e.verb;
    if (auto v = get_dbl(items, "weight")) { e.weight = *v; e.has_weight = true; }
    return e;
}

} // namespace

// ── Public API ────────────────────────────────────────────────────────────────

std::unique_ptr<GmlGraph> parse_gml(const std::string& text) {
    Lexer lex(text);

    Token t1 = lex.next();
    if (t1.kind != TokKind::IDENT || t1.str != "graph") return nullptr;
    Token t2 = lex.next();
    if (t2.kind != TokKind::LBRACKET) return nullptr;

    auto items = parse_body(lex);

    auto g = std::make_unique<GmlGraph>();
    if (auto v = get_int(items, "directed")) g->directed = (*v != 0);
    if (auto v = get_int(items, "id"))       g->id       = *v;
    if (auto v = get_str(items, "label"))    g->label    = *v;
    if (auto v = get_str(items, "comment"))  g->comment  = *v;

    for (const auto& rb : items) {
        if (!rb.is_block) continue;
        if (rb.key == "node") g->nodes.push_back(build_node(rb.children));
        else if (rb.key == "edge") g->edges.push_back(build_edge(rb.children));
    }
    return g;
}

std::unique_ptr<GmlGraph> parse_gml_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return nullptr;
    std::string text((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    return parse_gml(text);
}

} // namespace khanary::world
