#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace khanary::world {

struct GmlNode {
    int64_t id    = 0;
    std::string label;
    double x = 0.0, y = 0.0, z = 0.0;
    double confidence = 1.0;
    double static_weight = 1.0;
    double health = 1.0;
    bool has_x = false, has_y = false, has_z = false;
};

struct GmlEdge {
    int64_t source = 0;
    int64_t target = 0;
    std::string label;
    std::string verb;
    double weight  = 1.0;
    bool has_weight = false;
};

struct GmlGraph {
    bool directed = false;
    int64_t id = 0;
    std::string label;
    std::string comment;
    std::vector<GmlNode> nodes;
    std::vector<GmlEdge> edges;
};

// Returns nullptr on parse failure.
std::unique_ptr<GmlGraph> parse_gml(const std::string& text);
std::unique_ptr<GmlGraph> parse_gml_file(const std::string& path);

} // namespace khanary::world
