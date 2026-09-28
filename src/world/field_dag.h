#pragma once
#include "world_state.h"
#include "../micronaut/micronaut_registry.h"
#include <string>
#include <vector>
#include <array>

namespace khanary::world {

// One entry in a tensor or pipeline-stage fold_map.
struct FoldEntry {
    std::string phase_name;
    std::string hook;     // "on_read" | "on_write" | "on_gc" | ""
    std::string monitor;  // condition expression e.g. "gradient_norm > 10.0", or ""
    std::string action;   // "warn_clip" | "promote_checkpoint" | ""
};

struct FieldNode {
    // "track" = micronaut (one dominant phase, circle in SVG)
    // "tensor" = living tensor (multi-phase arc in SVG)
    // "pipeline_stage" = C6 pipeline stage (rect in SVG)
    // "ontology" = top-level ontology class (rect in SVG)
    std::string kind = "track";

    std::string track_id;
    std::string phase_name;  // dominant / primary phase
    std::string face_name;
    std::string block_name;
    std::string category;    // "executive"|"cognitive"|"storage"|"creative"|""
    float       confidence;
    float       static_weight = 1.0f;
    float       health = 1.0f;
    float       wcr_prior = 1.0f;
    float       x, y, z;
    std::string vector_source = "world_position";
    bool        coordinator;
    std::vector<std::string> effects;
    // SVG-3D weight layer: intensity per AtomicDOMBlock (Perceive..Evolve)
    std::array<float, 7> block_intensities{};
    // Multi-phase presence declaration (tensors + pipeline stages)
    std::vector<FoldEntry> fold_map;
};

// Directed (or undirected) arc in the semantic field graph
struct FieldEdge {
    std::string from;
    std::string to;
    // "delegate" | "horizontal_fold" | "vertical_fold"
    std::string type;
    // Relation token when built from relations data (canonical + compatibility aliases).
    // Canonical examples: @is_a, @part_of, @supports, @requires, @governs, @delegates_to.
    std::string verb;
    // Canonical phase hint for verb relation family (Pop/Wo/Sek/Ch'en).
    std::string relation_phase;
    float       weight;       // cos(Δθ): 1.0 / 0.5 / -1.0 / ...
    float       delta_theta;  // degrees
    bool        xcfe_required;
    bool        directed = true;  // false → horizontal relation (Δθ=0)
};

struct FieldSvgProjectionNode {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float phase_angle = 0.0f;
    std::array<float, 7> bar_heights{};
    bool  bar_heights_valid = false;
};

struct FieldSvgProjection {
    std::vector<FieldSvgProjectionNode> nodes;
    bool gpu_projected = false;
};

// Builds the 2-D semantic field DAG from WorldState + MicronautRegistry.
// Nodes = tracks; edges = declared delegate arcs + C6 fold arcs.
// Serializes to XML-DAG format suitable for PRIMEOS WebView2 rendering.
class FieldDag {
public:
    FieldDag(const WorldState& ws, const micronaut::MicronautRegistry& reg);

    void build();
    // Overlay GML graph nodes/edges onto the DAG. Must be called AFTER build().
    void load_gml(const std::string& path);
    std::string to_xml() const;
    // SVG output: field substrate + tensor arcs (kind=tensor) + track circles + SVG-3D blocks
    std::string to_svg(const FieldSvgProjection* projection = nullptr) const;

    // Register a living tensor node with its fold_map phases.
    // Adds the node and its @live / @self_aware entries; no edges auto-added.
    void add_tensor(const std::string& id,
                    const std::string& shape,
                    std::vector<FoldEntry> entries);

    const std::vector<FieldNode>& nodes() const { return nodes_; }
    const std::vector<FieldEdge>& edges() const { return edges_; }

private:
    const WorldState& ws_;
    const micronaut::MicronautRegistry& reg_;
    std::vector<FieldNode> nodes_;
    std::vector<FieldEdge> edges_;

    static std::string xml_escape(std::string_view s);

    // SVG helpers
    static constexpr float SVG_CX    = 512.0f;
    static constexpr float SVG_CY    = 400.0f;
    static constexpr float SVG_SCALE = 150.0f;

    struct SVGPos { float x, y; };
    // Track position (includes z-slot diagonal offset)
    static SVGPos to_svg_pos(const FieldNode& n);
    // Raw C₆ phase position on circle, no z-offset (for tensor arcs)
    static SVGPos phase_svg_pos(kuhul::Phase p);
    static std::string svg_edge_path(const SVGPos& a, const SVGPos& b);
    std::string svg_tensor_block(
        const FieldNode& n,
        const SVGPos& p,
        const FieldSvgProjectionNode* projected) const;
    std::string svg_tensor_arc(const FieldNode& n) const;
};

} // namespace khanary::world
