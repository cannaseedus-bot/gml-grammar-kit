#include "field_dag.h"
#include "gml_parser.h"
#include "../micronaut/relation_isa.h"
#include "../kuhul/phase_engine.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace khanary::world {

namespace {

std::string lower_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contains_any(const std::string& haystack, std::initializer_list<const char*> needles) {
    for (const char* n : needles) {
        if (haystack.find(n) != std::string::npos) return true;
    }
    return false;
}

std::string gml_source_token(const std::string& path) {
    const auto base_pos = path.find_last_of("/\\");
    const size_t base_start = (base_pos == std::string::npos) ? 0 : base_pos + 1;
    const auto dot_pos = path.find_last_of('.');
    const size_t base_end = (dot_pos != std::string::npos && dot_pos > base_start)
        ? dot_pos
        : path.size();

    std::string token = path.substr(base_start, base_end - base_start);
    if (token.empty()) token = "graph";

    for (char& c : token) {
        const auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '-' || c == '_') {
            c = static_cast<char>(std::tolower(uc));
        } else {
            c = '_';
        }
    }
    return token;
}

void assign_phase_face_block(FieldNode& n, kuhul::Phase p) {
    n.phase_name = kuhul::phase_name(p);
    const auto fi = static_cast<size_t>(p);
    if (fi < KCUBE_FACES.size()) {
        n.face_name  = KCUBE_FACES[fi].geometry;
        n.block_name = KCUBE_FACES[fi].block;
    } else {
        n.face_name  = n.phase_name;
        n.block_name = "";
    }
}

std::string classify_ontology_bucket(const micronaut::MicronautEntry& e) {
    std::string blob = lower_copy(e.id + " " + e.category + " " + e.description);
    for (const auto& g : e.governs) {
        blob += ' ';
        blob += lower_copy(g);
    }

    if (contains_any(blob, {"coordinator", "orchestr", "mission", "objective", "goal", "plan", "executive"}))
        return "Organization";
    if (contains_any(blob, {"perspective", "view", "concern", "scenario", "profile"}))
        return "Perspective";
    if (contains_any(blob, {"reason", "infer", "logic", "thought", "theory", "mind", "conscious", "evaluate", "verify"}))
        return "Function";
    if (contains_any(blob, {"knowledge", "data", "metadata", "memory", "search", "research", "citation", "information", "retrieve", "librarian"}))
        return "Information";
    if (contains_any(blob, {"component", "app", "wizard", "factory", "code", "build", "template", "module", "service"}))
        return "Component";
    if (contains_any(blob, {"communic", "protocol", "message", "meeting", "email", "translate"}))
        return "Communication";
    if (contains_any(blob, {"connect", "router", "route", "dispatch", "bridge", "adapter", "tool-call", "connector"}))
        return "Connector";
    if (contains_any(blob, {"environment", "world", "runtime", "physical", "sandbox", "gpu"}))
        return "Environment";

    switch (e.primary_phase) {
        case kuhul::Phase::Pop:  return "Information";
        case kuhul::Phase::Wo:   return "Component";
        case kuhul::Phase::Yax:  return "Perspective";
        case kuhul::Phase::Sek:  return "Function";
        case kuhul::Phase::Chen: return "Organization";
        case kuhul::Phase::Xul:  return "Environment";
    }
    return "Information";
}

FieldEdge make_ontology_edge(const std::string& from,
                             const std::string& to,
                             const std::string& verb,
                             bool directed,
                             float weight = 1.0f,
                             float delta_theta = 0.0f) {
    FieldEdge e;
    e.from = from;
    e.to = to;
    e.type = "ontology";
    e.verb = micronaut::canonical_relation(verb);
    if (!micronaut::is_supported_relation(e.verb))
        e.verb = micronaut::infer_relation_from_lexical(verb);
    e.relation_phase = micronaut::relation_phase_hint(e.verb);
    e.directed = directed;
    e.weight = weight;
    e.delta_theta = delta_theta;
    e.xcfe_required = false;
    return e;
}

} // namespace

FieldDag::FieldDag(const WorldState& ws, const micronaut::MicronautRegistry& reg)
    : ws_(ws), reg_(reg)
{}

// ── build ─────────────────────────────────────────────────────────────────────

void FieldDag::build() {
    nodes_.clear();
    edges_.clear();

    // Index WorldState nodes by track_id for quick lookup
    std::unordered_map<std::string, const TrackWorldNode*> node_map;
    for (const auto& wn : ws_.nodes())
        node_map[wn.track_id] = &wn;

    // Phase index: phase → list of track_ids at that phase
    std::unordered_map<int, std::vector<std::string>> phase_to_tracks;
    // Track → ontology class link
    std::unordered_map<std::string, std::string> track_to_ontology;

    // Build FieldNodes from WorldState (which is already built from registry)
    for (const auto& id : reg_.ids()) {
        const auto* e = reg_.get(id);
        if (!e) continue;
        auto wn_it = node_map.find(id);
        if (wn_it == node_map.end()) continue;
        const auto* wn = wn_it->second;

        FieldNode fn;
        fn.track_id    = id;
        fn.phase_name  = kuhul::phase_name(e->primary_phase);
        fn.block_name  = std::string(block_name(wn->active_block));
        fn.confidence  = std::clamp(e->confidence, 0.0f, 1.0f);
        fn.static_weight = std::clamp(e->static_weight, 0.0f, 1.0f);
        fn.health = std::clamp(e->health, 0.0f, 1.0f);
        fn.wcr_prior = std::clamp(fn.static_weight * fn.confidence * fn.health, 0.0f, 1.0f);
        fn.x           = wn->x;
        fn.y           = wn->y;
        fn.z           = wn->z;
        fn.vector_source = e->vector_meta.declared ? e->vector_meta.source : "phase_default";
        fn.kind              = "track";
        fn.category          = e->category;
        fn.coordinator       = e->sandbox.coordinator;
        fn.effects           = e->sandbox.effects;
        fn.block_intensities = wn->block_intensities;

        auto fi = static_cast<size_t>(e->primary_phase);
        fn.face_name = (fi < KCUBE_FACES.size())
                           ? std::string(KCUBE_FACES[fi].geometry)
                           : fn.phase_name;

        phase_to_tracks[static_cast<int>(e->primary_phase)].push_back(id);
        track_to_ontology[id] = classify_ontology_bucket(*e);
        nodes_.push_back(std::move(fn));
    }

    // ── Edge set 1: relational verb edges + legacy delegate arcs ────────────
    for (const auto& id : reg_.ids()) {
        const auto* e = reg_.get(id);
        if (!e) continue;

        // Typed relational edges from the `relations` table
        for (const auto& rel : e->relations) {
            if (!node_map.count(rel.target_id)) continue;
            kuhul::Phase target_phase = kuhul::Phase::Pop;
            if (const auto* te = reg_.get(rel.target_id))
                target_phase = te->primary_phase;
            float dtheta = static_cast<float>(
                kuhul::phase_distance(e->primary_phase, target_phase)) * 60.0f;

            FieldEdge edge;
            edge.from          = id;
            edge.to            = rel.target_id;
            edge.verb          = micronaut::canonical_relation(rel.verb);
            if (!micronaut::is_supported_relation(edge.verb))
                edge.verb = micronaut::infer_relation_from_lexical(rel.verb);
            edge.relation_phase = micronaut::relation_phase_hint(edge.verb);
            edge.directed      = rel.directed;
            edge.type          = rel.directed ? "delegate" : "horizontal_fold";
            edge.weight        = rel.directed ? std::cos(dtheta * 3.14159f / 180.0f) : 1.0f;
            edge.delta_theta   = rel.directed ? dtheta : 0.0f;
            edge.xcfe_required = rel.directed && !micronaut::relation_matches(edge.verb, "@delegates_to");
            edges_.push_back(std::move(edge));
        }

        // Legacy plain-delegate fallback (no verb — used when relations is empty)
        if (e->relations.empty()) {
            for (const auto& target : e->sub_micronauts) {
                if (!node_map.count(target)) continue;
                FieldEdge edge;
                edge.from          = id;
                edge.to            = target;
                edge.type          = "delegate";
                edge.verb          = "@delegates_to";
                edge.relation_phase = micronaut::relation_phase_hint(edge.verb);
                edge.weight        = 1.0f;
                edge.delta_theta   = 0.0f;
                edge.xcfe_required = false;
                edges_.push_back(std::move(edge));
            }
        }
    }

    // ── Edge set 2: C₆ vertical fold arcs (adjacent phases) ─────────────────
    // Directed: phase_i → next_phase; arc weight = cos(π/3) = 0.5
    for (const auto& id_a : reg_.ids()) {
        const auto* ea = reg_.get(id_a);
        if (!ea) continue;
        kuhul::Phase next_p = kuhul::next_phase(ea->primary_phase);
        auto it = phase_to_tracks.find(static_cast<int>(next_p));
        if (it == phase_to_tracks.end()) continue;
        for (const auto& id_b : it->second) {
            FieldEdge edge;
            edge.from          = id_a;
            edge.to            = id_b;
            edge.type          = "vertical_fold";
            edge.weight        = 0.5f;  // cos(π/3)
            edge.delta_theta   = 60.0f;
            edge.xcfe_required = true;
            edges_.push_back(std::move(edge));
        }
    }

    // ── Edge set 3: horizontal fold arcs (same phase, Δθ=0, weight=1.0) ─────
    // Undirected pair emitted once (from < to lexicographically)
    for (const auto& [pi, ids] : phase_to_tracks) {
        if (ids.size() < 2) continue;
        for (size_t i = 0; i < ids.size(); ++i) {
            for (size_t j = i + 1; j < ids.size(); ++j) {
                FieldEdge edge;
                edge.from          = ids[i];
                edge.to            = ids[j];
                edge.type          = "horizontal_fold";
                edge.weight        = 1.0f;  // cos(0)
                edge.delta_theta   = 0.0f;
                edge.xcfe_required = false;
                edges_.push_back(edge);
                // Reverse direction (symmetric)
                std::swap(edge.from, edge.to);
                edges_.push_back(std::move(edge));
            }
        }
    }

    // ── Ontology overlay: MBED top-level classes + typed links ──────────────
    auto add_ontology_node =
        [&](const char* id,
            kuhul::Phase phase,
            float x, float y, float z,
            std::initializer_list<const char*> attrs) {
            FieldNode n;
            n.kind = "ontology";
            n.track_id = id;
            n.category = "ontology";
            n.confidence = 1.0f;
            n.static_weight = 1.0f;
            n.health = 1.0f;
            n.wcr_prior = 1.0f;
            n.x = x;
            n.y = y;
            n.z = z;
            n.vector_source = "ontology_static";
            n.coordinator = false;
            assign_phase_face_block(n, phase);
            for (const char* a : attrs) n.effects.push_back(a);
            nodes_.push_back(std::move(n));
        };

    add_ontology_node("Organization", kuhul::Phase::Chen,  0.05f,  1.85f, 0.0f,
                      {"mission", "requirements", "objectives", "goals", "scenarios"});
    add_ontology_node("Perspective",  kuhul::Phase::Yax,   2.10f,  1.45f, 0.0f,
                      {"contains_objects", "defines", "exposes_concerns"});
    add_ontology_node("Function",     kuhul::Phase::Sek,  -2.05f,  0.25f, 0.0f,
                      {"logical_structure", "behavior", "interfaces", "constraints"});
    add_ontology_node("Information",  kuhul::Phase::Pop,   2.05f,  0.20f, 0.0f,
                      {"data", "metadata", "rules"});
    add_ontology_node("Component",    kuhul::Phase::Wo,    0.10f, -0.95f, 0.0f,
                      {"type", "attributes", "ports", "location"});
    add_ontology_node("Environment",  kuhul::Phase::Xul,   2.00f, -1.00f, 0.0f,
                      {"physical_environs", "attributes"});
    add_ontology_node("Communication",kuhul::Phase::Yax,  -1.95f, -1.45f, 0.0f,
                      {"protocol_stack", "standards"});
    add_ontology_node("Connector",    kuhul::Phase::Wo,    1.20f, -1.85f, 0.0f,
                      {"type", "attributes"});

    std::unordered_set<std::string> ontology_ids = {
        "Organization", "Perspective", "Function", "Information",
        "Component", "Environment", "Communication", "Connector"
    };

    for (const auto& [track_id, onto_id] : track_to_ontology) {
        if (!ontology_ids.count(onto_id)) continue;
        edges_.push_back(make_ontology_edge(track_id, onto_id, "@related_to", false, 1.0f, 0.0f));
    }

    // Core ontology relationships (from MBED top-level pattern).
    edges_.push_back(make_ontology_edge("Organization", "Function", "@part_of", true, 0.85f, 30.0f));
    edges_.push_back(make_ontology_edge("Organization", "Component", "@delegates_to", true, 0.85f, 30.0f));
    edges_.push_back(make_ontology_edge("Perspective", "Organization", "@related_to", false, 1.0f, 0.0f));
    edges_.push_back(make_ontology_edge("Perspective", "Information", "@related_to", false, 1.0f, 0.0f));
    edges_.push_back(make_ontology_edge("Function", "Information", "@delegates_to", true, 0.8f, 30.0f));
    edges_.push_back(make_ontology_edge("Information", "Function", "@supports", true, 0.75f, 30.0f));
    edges_.push_back(make_ontology_edge("Function", "Component", "@delegates_to", true, 0.8f, 30.0f));
    edges_.push_back(make_ontology_edge("Communication", "Function", "@delegates_to", true, 0.8f, 30.0f));
    edges_.push_back(make_ontology_edge("Function", "Communication", "@part_of", true, 0.8f, 30.0f));
    edges_.push_back(make_ontology_edge("Communication", "Component", "@delegates_to", true, 0.8f, 30.0f));
    edges_.push_back(make_ontology_edge("Component", "Connector", "@delegates_to", true, 0.8f, 30.0f));
    edges_.push_back(make_ontology_edge("Connector", "Component", "@part_of", true, 0.8f, 30.0f));
    edges_.push_back(make_ontology_edge("Component", "Environment", "@governs", true, 0.8f, 30.0f));
}

// ── add_tensor ────────────────────────────────────────────────────────────────

void FieldDag::add_tensor(const std::string& id,
                          const std::string& /*shape*/,
                          std::vector<FoldEntry> entries)
{
    FieldNode fn;
    fn.kind       = "tensor";
    fn.track_id   = id;
    fn.confidence = 1.0f;
    fn.static_weight = 1.0f;
    fn.health = 1.0f;
    fn.wcr_prior = 1.0f;
    fn.vector_source = "tensor_projection";
    fn.fold_map   = std::move(entries);
    if (!fn.fold_map.empty()) {
        fn.phase_name = fn.fold_map[0].phase_name;
        kuhul::Phase ph = kuhul::Phase::Pop;
        kuhul::phase_from_name(fn.phase_name, ph);
        auto sp = phase_svg_pos(ph);
        fn.x = (sp.x - SVG_CX) / SVG_SCALE;
        fn.y = (SVG_CY - sp.y) / SVG_SCALE;
        fn.z = 0.0f;
        auto fi = static_cast<size_t>(ph);
        if (fi < KCUBE_FACES.size()) {
            fn.face_name  = std::string(KCUBE_FACES[fi].geometry);
            fn.block_name = std::string(KCUBE_FACES[fi].block);
        }
    }
    nodes_.push_back(std::move(fn));
}

// ── GML overlay ───────────────────────────────────────────────────────────────

void FieldDag::load_gml(const std::string& path) {
    auto g = parse_gml_file(path);
    if (!g) return;

    const std::string source_token = gml_source_token(path);
    const size_t N = g->nodes.size();
    constexpr double kPI = 3.14159265358979323846;

    // Build id → track_id map for edge resolution
    std::unordered_map<int64_t, std::string> id_to_tid;
    id_to_tid.reserve(N);

    for (size_t i = 0; i < N; ++i) {
        const auto& gn = g->nodes[i];

        // If the label names a registered track, bind to it directly —
        // no phantom node, edges will wire into the real track.
        if (!gn.label.empty() && reg_.get(gn.label)) {
            id_to_tid[gn.id] = gn.label;
            continue;
        }

        FieldNode fn;
        fn.kind        = "gml";
        fn.track_id    = "gml:" + source_token + ":" + std::to_string(gn.id);
        fn.phase_name  = "Pop";
        fn.confidence  = std::clamp(static_cast<float>(gn.confidence), 0.0f, 1.0f);
        fn.static_weight = std::clamp(static_cast<float>(gn.static_weight), 0.0f, 1.0f);
        fn.health = std::clamp(static_cast<float>(gn.health), 0.0f, 1.0f);
        fn.wcr_prior = std::clamp(fn.confidence * fn.static_weight * fn.health, 0.0f, 1.0f);
        fn.vector_source = "gml";
        fn.coordinator = false;
        if (!gn.label.empty()) fn.category = gn.label;

        if (gn.has_x && gn.has_y) {
            fn.x = static_cast<float>(gn.x);
            fn.y = static_cast<float>(gn.y);
            fn.z = gn.has_z ? static_cast<float>(gn.z) : 0.0f;
        } else {
            double angle = (N > 1)
                ? (2.0 * kPI * static_cast<double>(i) / static_cast<double>(N))
                : 0.0;
            fn.x = static_cast<float>(2.5 * std::cos(angle));
            fn.y = static_cast<float>(2.5 * std::sin(angle));
            fn.z = 0.0f;
        }

        id_to_tid[gn.id] = fn.track_id;
        nodes_.push_back(std::move(fn));
    }

    for (const auto& ge : g->edges) {
        auto sit = id_to_tid.find(ge.source);
        auto tit = id_to_tid.find(ge.target);
        if (sit == id_to_tid.end() || tit == id_to_tid.end()) continue;

        FieldEdge fe;
        fe.from          = sit->second;
        fe.to            = tit->second;
        fe.directed      = g->directed;
        fe.type          = g->directed ? "delegate" : "horizontal_fold";
        fe.weight        = ge.has_weight ? static_cast<float>(ge.weight) : 1.0f;
        fe.delta_theta   = 0.0f;
        fe.xcfe_required = false;

        const std::string relation_raw = !ge.verb.empty() ? ge.verb : ge.label;
        if (!relation_raw.empty()) {
            std::string typed = relation_raw;
            if (!typed.empty() && typed.front() == '@') {
                typed = micronaut::canonical_relation(typed);
            } else {
                typed = micronaut::infer_relation_from_lexical(relation_raw);
            }
            if (!typed.empty()) {
                fe.verb = typed;
                fe.relation_phase = micronaut::relation_phase_hint(typed);
                fe.xcfe_required = g->directed && !micronaut::relation_matches(typed, "@delegates_to");
            }
        }
        edges_.push_back(std::move(fe));
    }
}

// ── XML serialization ─────────────────────────────────────────────────────────

std::string FieldDag::xml_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;
        }
    }
    return out;
}

std::string FieldDag::to_xml() const {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    os << "<field version=\"1.0\" phase_cycle=\"C6\">\n";

    // ── faces section (K-CUBE face table) ────────────────────────────────────
    os << "  <faces>\n";
    for (const auto& face : KCUBE_FACES) {
        os << "    <face"
           << " id=\""        << xml_escape(face.geometry)            << "\""
           << " phase=\""     << xml_escape(kuhul::phase_name(face.phase)) << "\""
           << " block=\""     << xml_escape(face.block)               << "\""
           << " angle_deg=\"" << static_cast<int>(face.angle_deg)     << "\"";
        if (!face.extra_block.empty())
            os << " extra_block=\"" << xml_escape(face.extra_block) << "\"";
        os << "/>\n";
    }
    os << "  </faces>\n";

    // ── nodes ─────────────────────────────────────────────────────────────────
    os << "  <nodes>\n";
    // XCFE phase anchor nodes — @phase: typed, one per C6 face
    for (const auto& face : KCUBE_FACES) {
        kuhul::Phase next_p = kuhul::next_phase(face.phase);
        char abuf[16];
        std::snprintf(abuf, sizeof(abuf), "%.3f", kuhul::phase_angle(face.phase));
        os << "    <node"
           << " id=\"@phase:"   << xml_escape(kuhul::phase_name(face.phase)) << "\""
           << " type=\"xcfe_phase\""
           << " face=\""        << xml_escape(face.geometry) << "\""
           << " block=\""       << xml_escape(face.block)    << "\""
           << " angle_deg=\""   << static_cast<int>(face.angle_deg) << "\""
           << " angle_rad=\""   << abuf << "\""
           << " next=\"@phase:" << xml_escape(kuhul::phase_name(next_p)) << "\""
           << "/>\n";
    }
    for (const auto& n : nodes_) {
        os << "    <node"
           << " id=\"@node:"    << xml_escape(n.track_id)   << "\""
           << " kind=\""        << xml_escape(n.kind)        << "\""
           << " phase=\""       << xml_escape(n.phase_name) << "\""
           << " face=\""        << xml_escape(n.face_name)  << "\""
           << " block=\""       << xml_escape(n.block_name) << "\""
           << " confidence=\""  << n.confidence             << "\""
           << " static_weight=\"" << n.static_weight        << "\""
           << " health=\""      << n.health                 << "\""
           << " wcr_prior=\""   << n.wcr_prior              << "\""
           << " coordinator=\"" << (n.coordinator ? "true" : "false") << "\"";
        if (!n.category.empty())
           os << " category=\"" << xml_escape(n.category) << "\"";
        os << " vector_source=\"" << xml_escape(n.vector_source) << "\"";
        os << " x=\""           << n.x << "\""
           << " y=\""           << n.y << "\""
           << " z=\""           << n.z << "\"";
        os << ">\n";
        if (!n.effects.empty()) {
            os << "      <effects>";
            for (size_t i = 0; i < n.effects.size(); ++i) {
                if (i) os << ' ';
                os << xml_escape(n.effects[i]);
            }
            os << "</effects>\n";
        }
        os << "      <vector_meta"
           << " source=\"" << xml_escape(n.vector_source) << "\""
           << " qkv_role=\"K\""
           << " x=\"" << n.x << "\""
           << " y=\"" << n.y << "\""
           << " z=\"" << n.z << "\""
           << "/>\n";
        // SVG-3D weight layer: one float per AtomicDOMBlock
        os << "      <tensors";
        for (uint8_t bi = 0; bi < static_cast<uint8_t>(AtomicDOMBlock::COUNT); ++bi) {
            auto b = static_cast<AtomicDOMBlock>(bi);
            os << " " << block_name(b) << "=\"" << n.block_intensities[bi] << "\"";
        }
        os << "/>\n";
        // fold_map: multi-phase presence declaration (tensors + pipeline stages)
        if (!n.fold_map.empty()) {
            os << "      <fold_map>\n";
            for (const auto& fe : n.fold_map) {
                os << "        <entry phase=\"" << xml_escape(fe.phase_name) << "\"";
                if (!fe.hook.empty())    os << " hook=\""    << xml_escape(fe.hook)    << "\"";
                if (!fe.monitor.empty()) os << " monitor=\"" << xml_escape(fe.monitor) << "\"";
                if (!fe.action.empty())  os << " action=\""  << xml_escape(fe.action)  << "\"";
                os << "/>\n";
            }
            os << "      </fold_map>\n";
        }
        os << "    </node>\n";
    }
    os << "  </nodes>\n";

    // ── edges ─────────────────────────────────────────────────────────────────
    os << "  <edges>\n";
    for (const auto& e : edges_) {
        os << "    <edge"
           << " from=\"@node:"    << xml_escape(e.from)  << "\""
           << " to=\"@node:"      << xml_escape(e.to)    << "\""
           << " type=\""          << xml_escape(e.type)  << "\"";
        if (!e.verb.empty())
           os << " verb=\""       << xml_escape(e.verb)  << "\"";
        if (!e.relation_phase.empty())
           os << " relation_phase=\"" << xml_escape(e.relation_phase) << "\"";
        os << " weight=\""        << e.weight             << "\""
           << " delta_theta=\""   << static_cast<int>(e.delta_theta) << "\""
           << " xcfe_required=\"" << (e.xcfe_required ? "true" : "false") << "\""
           << " directed=\""      << (e.directed ? "true" : "false") << "\""
           << "/>\n";
    }
    // Phase anchor edges: @node:X → @phase:P
    for (const auto& n : nodes_) {
        os << "    <edge"
           << " from=\"@node:" << xml_escape(n.track_id)   << "\""
           << " to=\"@phase:"  << xml_escape(n.phase_name) << "\""
           << " type=\"phase_anchor\" arc_weight=\"1.000\" directed=\"true\""
           << "/>\n";
    }
    os << "  </edges>\n";

    os << "</field>\n";
    return os.str();
}

// ── SVG rendering ─────────────────────────────────────────────────────────────

// Block colors (ASX palette)
static constexpr const char* BLOCK_COLORS[7] = {
    "#22f2b3",  // perceive  → --asx-primary
    "#38bdf8",  // learn     → --asx-secondary
    "#a855f7",  // think     → --asx-accent
    "#facc15",  // evaluate  → --asx-warning
    "#4ade80",  // confidence→ --asx-success
    "#fb7185",  // memory    → --asx-danger
    "#fbbf24",  // evolve    → --asx-hazard
};

// Relational verb colors (ASX palette)
static const char* verb_color(const std::string& verb) {
    const auto rel = micronaut::canonical_relation(verb);
    if (rel == "@delegates_to" || rel == "@return_to" || rel == "@resumes")
        return "#38bdf8"; // runtime
    if (rel == "@is_a" || rel == "@part_of" || rel == "@has_a")
        return "#22c55e"; // structural semantic
    if (rel == "@related_to")
        return "#22f2b3"; // weak semantic association
    if (rel == "@causes" || rel == "@enables" || rel == "@prevents")
        return "#fb7185"; // causal
    if (rel == "@supports" || rel == "@contradicts" || rel == "@evidence_for")
        return "#facc15"; // epistemic evidence
    if (rel == "@requires" || rel == "@produces" || rel == "@depends_on")
        return "#a855f7"; // dependency/process
    if (rel == "@governs" || rel == "@redirect")
        return "#eab308"; // control scope / redirect
    if (rel == "@context" || rel == "@previous" || rel == "@next")
        return "#14b8a6"; // contextual chain
    return nullptr;
}

// Edge type colors (fallback when no verb is set)
static const char* edge_color(const std::string& type, const std::string& verb) {
    if (const char* vc = verb_color(verb)) return vc;
    if (type == "ontology")        return "#fbbf24";
    if (type == "delegate")        return "#22f2b3";
    if (type == "horizontal_fold") return "#38bdf8";
    return "#6b7280";  // vertical_fold — faint
}

static const char* ontology_fill(const std::string& id) {
    if (id == "Organization")  return "#0891b2";
    if (id == "Perspective")   return "#0e7490";
    if (id == "Function")      return "#a21caf";
    if (id == "Information")   return "#dc2626";
    if (id == "Component")     return "#0284c7";
    if (id == "Environment")   return "#0369a1";
    if (id == "Communication") return "#ca8a04";
    if (id == "Connector")     return "#2563eb";
    return "#334155";
}

FieldDag::SVGPos FieldDag::phase_svg_pos(kuhul::Phase p) {
    constexpr double kPI = 3.14159265358979323846;
    double angle = (2.0 * kPI / 6.0) * static_cast<int>(p);
    return {
        SVG_CX + static_cast<float>(SVG_SCALE * std::cos(angle)),
        SVG_CY - static_cast<float>(SVG_SCALE * std::sin(angle))
    };
}

FieldDag::SVGPos FieldDag::to_svg_pos(const FieldNode& n) {
    // World coords: x,y in [-2,2]; z is stacking offset (0, 0.5, 1.0, ...)
    // SVG: y-axis inverted; z adds a diagonal offset so stacked tracks are visible
    return {
        SVG_CX + n.x * SVG_SCALE + n.z * 22.0f,
        SVG_CY - n.y * SVG_SCALE - n.z * 12.0f
    };
}

std::string FieldDag::svg_edge_path(const SVGPos& a, const SVGPos& b) {
    // Bezier with midpoint control pulled slightly inward toward center
    float mx = (a.x + b.x) * 0.5f + (SVG_CX - (a.x + b.x) * 0.5f) * 0.15f;
    float my = (a.y + b.y) * 0.5f + (SVG_CY - (a.y + b.y) * 0.5f) * 0.15f;
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "M %.1f,%.1f Q %.1f,%.1f %.1f,%.1f",
        a.x, a.y, mx, my, b.x, b.y);
    return buf;
}

// Draw 7 isometric tensor bars above a node, colored by block type.
// Each bar = front face (rect) + top face (parallelogram).
// Bar height ∝ block_intensities[i]; zero-intensity bars are faint placeholders.
std::string FieldDag::svg_tensor_block(
    const FieldNode& n,
    const SVGPos& p,
    const FieldSvgProjectionNode* projected) const {
    std::ostringstream os;
    constexpr float BAR_W  = 7.0f;    // bar width
    constexpr float BAR_MIN = 3.0f;   // minimum visible bar height
    constexpr float BAR_MAX = 28.0f;  // max bar height
    constexpr float DEPTH  = 4.0f;    // isometric top-face depth
    constexpr float GAP    = 1.5f;
    const float total_w = 7 * (BAR_W + GAP) - GAP;
    const float start_x = p.x - total_w * 0.5f;

    for (uint8_t bi = 0; bi < 7; ++bi) {
        float h = BAR_MIN;
        float intensity = n.block_intensities[bi];
        if (projected && projected->bar_heights_valid) {
            h = std::max(BAR_MIN, projected->bar_heights[bi]);
            const float denom = std::max(0.001f, BAR_MAX - BAR_MIN);
            intensity = std::clamp((h - BAR_MIN) / denom, 0.0f, 1.0f);
        } else {
            h = std::max(BAR_MIN, n.block_intensities[bi] * BAR_MAX);
        }
        float bx = start_x + bi * (BAR_W + GAP);
        float by = p.y - 22.0f - h;   // base of bar (above node circle)
        const char* col = BLOCK_COLORS[bi];
        float alpha = intensity < 0.05f ? 0.18f : 0.90f;

        // Front face
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\""
            " fill=\"%s\" fill-opacity=\"%.2f\" rx=\"1\"/>",
            bx, by, BAR_W, h, col, alpha);
        os << buf;

        // Top face (isometric lid)
        if (intensity >= 0.05f) {
            // parallelogram: top-left, top-right, shifted-right, shifted-left
            std::snprintf(buf, sizeof(buf),
                "<polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f %.1f,%.1f\""
                " fill=\"%s\" fill-opacity=\"0.55\"/>",
                bx,          by,
                bx + BAR_W,  by,
                bx + BAR_W + DEPTH * 0.7f, by - DEPTH * 0.5f,
                bx + DEPTH * 0.7f,         by - DEPTH * 0.5f,
                col);
            os << buf;
        }
    }
    return os.str();
}

// Render a tensor arc: shaded polygon connecting fold_map phase positions.
// Drawn behind track circles at Z-layer 0.5 (after field substrate, before edges).
std::string FieldDag::svg_tensor_arc(const FieldNode& n) const {
    if (n.fold_map.empty()) return {};
    std::ostringstream os;

    // Build point list from fold_map phases
    std::vector<SVGPos> pts;
    for (const auto& fe : n.fold_map) {
        kuhul::Phase ph = kuhul::Phase::Pop;
        kuhul::phase_from_name(fe.phase_name, ph);
        pts.push_back(phase_svg_pos(ph));
    }

    // Filled polygon (the tensor's multi-phase footprint)
    os << "<polygon points=\"";
    for (const auto& p : pts) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f,%.1f ", p.x, p.y);
        os << buf;
    }
    os << "\" fill=\"#38bdf8\" fill-opacity=\"0.08\""
       << " stroke=\"#38bdf8\" stroke-width=\"1.2\" stroke-opacity=\"0.55\""
       << " stroke-dasharray=\"4 3\"/>\n";

    // Small diamond at each fold_map position, colored by hook type
    for (size_t i = 0; i < n.fold_map.size(); ++i) {
        const auto& fe = n.fold_map[i];
        const SVGPos& p = pts[i];
        // hook color: on_read=primary, on_write=warning, monitor=danger, other=secondary
        const char* dc = "#38bdf8";
        if (fe.hook == "on_read")       dc = "#22f2b3";
        else if (fe.hook == "on_write") dc = "#facc15";
        else if (!fe.monitor.empty())   dc = "#fb7185";
        else if (fe.hook == "save_checkpoint" || fe.hook == "on_gc") dc = "#fbbf24";

        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "<polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f %.1f,%.1f\""
            " fill=\"%s\" opacity=\"0.85\"/>\n",
            p.x,     p.y - 5,
            p.x + 5, p.y,
            p.x,     p.y + 5,
            p.x - 5, p.y,
            dc);
        os << buf;

        // Hook annotation label
        if (!fe.hook.empty() || !fe.monitor.empty()) {
            const char* lbl = fe.hook.empty() ? "monitor" : fe.hook.c_str();
            std::snprintf(buf, sizeof(buf),
                "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\""
                " font-family=\"ui-monospace,monospace\" font-size=\"7\""
                " fill=\"%s\" opacity=\"0.75\">@%s</text>\n",
                p.x, p.y - 9.0f, dc, lbl);
            os << buf;
        }
    }

    // Centroid label (tensor id)
    float cx = 0, cy = 0;
    for (const auto& p : pts) { cx += p.x; cy += p.y; }
    cx /= (float)pts.size();
    cy /= (float)pts.size();
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "<text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\""
        " font-family=\"ui-monospace,monospace\" font-size=\"8\""
        " fill=\"#38bdf8\" opacity=\"0.7\">%s</text>\n",
        cx, cy + 4.0f,
        xml_escape(n.track_id).c_str());
    os << buf;

    return os.str();
}

std::string FieldDag::to_svg(const FieldSvgProjection* projection) const {
    // Build position map
    std::unordered_map<std::string, SVGPos> pos_map;
    std::unordered_map<std::string, const FieldSvgProjectionNode*> projected_nodes;
    const bool has_projection =
        projection && projection->nodes.size() == nodes_.size();

    for (size_t i = 0; i < nodes_.size(); ++i) {
        const auto& n = nodes_[i];
        if (has_projection) {
            const auto& pn = projection->nodes[i];
            pos_map[n.track_id] = {pn.x, pn.y};
            projected_nodes[n.track_id] = &pn;
        } else {
            pos_map[n.track_id] = to_svg_pos(n);
        }
    }

    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    os << "<svg xmlns=\"http://www.w3.org/2000/svg\""
       << " width=\"1024\" height=\"768\" viewBox=\"0 0 1024 768\">\n";
    os << "<title>KHANARY K-CUBE Semantic Field</title>\n";
    if (has_projection && projection->gpu_projected)
        os << "<desc>Field SVG projected via svg_compute.hlsl runtime path.</desc>\n";

    // ── Styles ────────────────────────────────────────────────────────────────
    os << R"(<style>
@keyframes pulse{0%,100%{opacity:.5}50%{opacity:1}}
@keyframes flow{0%{stroke-dashoffset:0}100%{stroke-dashoffset:-160}}
.fn{animation:pulse 2.5s ease-in-out infinite}
.ge{animation:flow 4s linear infinite}
</style>)";

    // ── Defs ──────────────────────────────────────────────────────────────────
    os << "<defs>"
       << "<radialGradient id=\"bg\" cx=\"50%\" cy=\"50%\" r=\"60%\">"
       << "<stop offset=\"0%\" stop-color=\"#050b1c\" stop-opacity=\"1\"/>"
       << "<stop offset=\"100%\" stop-color=\"#020617\" stop-opacity=\"1\"/>"
       << "</radialGradient>"
       << "<pattern id=\"grid\" width=\"40\" height=\"40\" patternUnits=\"userSpaceOnUse\">"
       << "<path d=\"M40 0L0 0 0 40\" fill=\"none\" stroke=\"#22f2b3\" stroke-width=\"0.25\" opacity=\"0.12\"/>"
       << "</pattern>"
       << "</defs>\n";

    // ── Layer 0: background + grid ────────────────────────────────────────────
    os << "<rect width=\"1024\" height=\"768\" fill=\"url(#bg)\"/>\n";
    os << "<rect width=\"1024\" height=\"768\" fill=\"url(#grid)\"/>\n";

    // ── Layer 0.5: tensor arcs (kind=tensor, multi-phase footprints) ─────────
    {
        bool any = false;
        for (const auto& n : nodes_) {
            if (n.kind == "tensor" && !n.fold_map.empty()) { any = true; break; }
        }
        if (any) {
            os << "<g id=\"tensor-arcs\">\n";
            for (const auto& n : nodes_)
                if (n.kind == "tensor") os << svg_tensor_arc(n);
            os << "</g>\n";
        }
    }

    // ── Layer 1: edges ────────────────────────────────────────────────────────
    os << "<g id=\"edges\" opacity=\"0.70\">\n";
    auto edge_pass = [](const FieldEdge& e) -> int {
        if (e.type == "vertical_fold")   return 0;
        if (e.type == "horizontal_fold") return 1;
        if (e.type == "ontology")        return 2;
        if (e.type == "delegate")        return 3;
        return 2;
    };
    // Draw from faint substrate edges to strong dispatch edges.
    for (int pass = 0; pass < 4; ++pass) {
        for (const auto& e : edges_) {
            if (edge_pass(e) != pass) continue;
            auto it_a = pos_map.find(e.from);
            auto it_b = pos_map.find(e.to);
            if (it_a == pos_map.end() || it_b == pos_map.end()) continue;
            const char* col = edge_color(e.type, e.verb);
            float op = 0.5f;
            float sw = 1.0f;
            if (e.type == "vertical_fold")      { op = 0.22f; sw = 1.0f; }
            else if (e.type == "horizontal_fold"){ op = 0.45f; sw = 1.0f; }
            else if (e.type == "ontology")      { op = 0.58f; sw = 1.2f; }
            else if (e.type == "delegate")      { op = 0.82f; sw = 1.5f; }

            bool is_relate = (micronaut::relation_matches(e.verb, "@related_to") || !e.directed);
            auto path = svg_edge_path(it_a->second, it_b->second);
            char buf[512];
            const char* dash = "5 4";
            if (is_relate) dash = "6 3 2 3";
            else if (e.type == "ontology") dash = "4 3";
            std::snprintf(buf, sizeof(buf),
                "  <path d=\"%s\" stroke=\"%s\" stroke-width=\"%.1f\""
                " fill=\"none\" stroke-dasharray=\"%s\""
                " opacity=\"%.2f\" class=\"ge\"/>\n",
                path.c_str(), col, sw, dash, op);
            os << buf;
        }
    }
    os << "</g>\n";

    // ── Layer 2: SVG-3D tensor blocks ─────────────────────────────────────────
    os << "<g id=\"tensors\">\n";
    for (const auto& n : nodes_) {
        if (n.kind != "track") continue;
        auto it = pos_map.find(n.track_id);
        if (it == pos_map.end()) continue;
        const FieldSvgProjectionNode* pn = nullptr;
        auto pit = projected_nodes.find(n.track_id);
        if (pit != projected_nodes.end()) pn = pit->second;
        os << "  <g id=\"t_" << xml_escape(n.track_id) << "\">\n";
        os << "  " << svg_tensor_block(n, it->second, pn) << "\n";
        os << "  </g>\n";
    }
    os << "</g>\n";

    // ── Layer 3: field nodes ──────────────────────────────────────────────────
    os << "<g id=\"nodes\">\n";
    for (const auto& n : nodes_) {
        auto it = pos_map.find(n.track_id);
        if (it == pos_map.end()) continue;
        const SVGPos& p = it->second;

        kuhul::Phase ph = kuhul::Phase::Pop;
        kuhul::phase_from_name(n.phase_name, ph);
        const char* col = BLOCK_COLORS[static_cast<uint8_t>(phase_to_block(ph))];

        char buf[1024];
        if (n.kind == "ontology") {
            const float w = 152.0f;
            const float h = 44.0f;
            const float x = p.x - w * 0.5f;
            const float y = p.y - h * 0.5f;
            const char* fill = ontology_fill(n.track_id);
            std::snprintf(buf, sizeof(buf),
                "  <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" rx=\"5\""
                " fill=\"%s\" fill-opacity=\"0.70\" stroke=\"#e2e8f0\" stroke-width=\"0.8\"/>\n",
                x, y, w, h, fill);
            os << buf;
            std::snprintf(buf, sizeof(buf),
                "  <text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\""
                " font-family=\"ui-monospace,monospace\" font-size=\"10\""
                " fill=\"#f8fafc\" font-weight=\"bold\">%s</text>\n",
                p.x, p.y - 4.0f, xml_escape(n.track_id).c_str());
            os << buf;
            std::string attrs;
            for (size_t i = 0; i < std::min<size_t>(3, n.effects.size()); ++i) {
                if (i) attrs += " | ";
                attrs += n.effects[i];
            }
            if (n.effects.size() > 3) attrs += " | ...";
            if (!attrs.empty()) {
                std::snprintf(buf, sizeof(buf),
                    "  <text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\""
                    " font-family=\"ui-monospace,monospace\" font-size=\"7\""
                    " fill=\"#e2e8f0\" opacity=\"0.9\">%s</text>\n",
                    p.x, p.y + 10.0f, xml_escape(attrs).c_str());
                os << buf;
            }
            continue;
        }

        // Outer ring
        std::snprintf(buf, sizeof(buf),
            "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"18\" fill=\"none\""
            " stroke=\"%s\" stroke-width=\"1.5\" opacity=\"0.7\" class=\"fn\"/>\n",
            p.x, p.y, col);
        os << buf;
        // Inner dot
        std::snprintf(buf, sizeof(buf),
            "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"5\" fill=\"%s\"/>\n",
            p.x, p.y, col);
        os << buf;
        // Coordinator marker
        if (n.coordinator) {
            std::snprintf(buf, sizeof(buf),
                "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"24\" fill=\"none\""
                " stroke=\"%s\" stroke-width=\"0.8\" stroke-dasharray=\"3 3\" opacity=\"0.4\"/>\n",
                p.x, p.y, col);
            os << buf;
        }
        // Label
        std::snprintf(buf, sizeof(buf),
            "  <text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\""
            " font-family=\"ui-monospace,monospace\" font-size=\"9\""
            " fill=\"%s\">%s</text>\n",
            p.x, p.y + 30.0f, col, xml_escape(n.track_id).c_str());
        os << buf;
        std::snprintf(buf, sizeof(buf),
            "  <text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\""
            " font-family=\"ui-monospace,monospace\" font-size=\"7\""
            " fill=\"%s\" opacity=\"0.6\">%s</text>\n",
            p.x, p.y + 40.0f, col, xml_escape(n.phase_name).c_str());
        os << buf;
    }
    os << "</g>\n";

    // ── HUD: legend ───────────────────────────────────────────────────────────
    os << "<g transform=\"translate(16,16)\">\n"
       << "  <rect width=\"180\" height=\"112\" rx=\"5\""
       << "  fill=\"rgba(2,6,23,0.88)\" stroke=\"#22f2b3\" stroke-width=\"0.8\"/>\n"
       << "  <text x=\"10\" y=\"16\" font-family=\"ui-monospace,monospace\""
       << "  font-size=\"9\" fill=\"#22f2b3\" font-weight=\"bold\">K-CUBE FIELD</text>\n";
    const char* bnames[7] = {"perceive","learn","think","evaluate","confidence","memory","evolve"};
    for (int i = 0; i < 7; ++i) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  <rect x=\"10\" y=\"%d\" width=\"8\" height=\"8\" fill=\"%s\"/>"
            "  <text x=\"24\" y=\"%d\" font-family=\"ui-monospace,monospace\""
            "  font-size=\"8\" fill=\"#e5f2ff\">%s</text>\n",
            24 + i * 12, BLOCK_COLORS[i], 32 + i * 12, bnames[i]);
        os << buf;
    }
    os << "</g>\n";

    os << "</svg>\n";
    return os.str();
}

} // namespace khanary::world
