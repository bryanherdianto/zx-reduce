#pragma once

#include "core/graph.hpp"
#include <vector>
#include <set>
#include <map>

namespace zx {

struct GadgetFusionMatch {
    VertexId leaf_keep;
    VertexId axel_keep;
    Phase new_phase;
    std::vector<VertexId> leaves_remove;
    std::vector<VertexId> axels_remove;
};

class GadgetRule {
public:
    static std::vector<GadgetFusionMatch> find_fuse(const Graph& g) {
        std::vector<GadgetFusionMatch> matches;

        std::unordered_set<VertexId> boundary_set(g.inputs.begin(), g.inputs.end());
        boundary_set.insert(g.outputs.begin(), g.outputs.end());

        // Find all phase gadgets: leaf v has degree 1, non-Clifford phase (t-like).
        // Connected to an axel/hub h with Pauli phase (0 or pi).
        std::map<std::set<VertexId>, std::vector<std::pair<VertexId, VertexId>>> gadget_groups;

        for (size_t i = 0; i < g.vertices.size(); ++i) {
            if (!g.vertices[i].active) continue;
            VertexId v = static_cast<VertexId>(i);
            if (g.vertices[v].type != VertexType::Z) continue;
            if (boundary_set.count(v)) continue;

            if (g.vertices[v].phase.is_t_like() && g.degree(v) == 1) {
                VertexId h = g.neighbors(v)[0];
                if (boundary_set.count(h) || g.vertices[h].type != VertexType::Z) continue;
                if (!g.vertices[h].phase.is_zero() && !g.vertices[h].phase.is_pauli()) continue;

                // Targets of the gadget are neighbors of h other than v
                std::set<VertexId> targets;
                bool valid_targets = true;
                for (VertexId t : g.neighbors(h)) {
                    if (t != v) {
                        if (boundary_set.count(t) || g.vertices[t].type != VertexType::Z) {
                            valid_targets = false;
                            break;
                        }
                        targets.insert(t);
                    }
                }
                if (valid_targets && !targets.empty()) {
                    gadget_groups[targets].push_back({v, h});
                }
            }
        }

        for (auto& [targets, g_list] : gadget_groups) {
            if (g_list.size() >= 2) {
                VertexId leaf_keep = g_list[0].first;
                VertexId axel_keep = g_list[0].second;

                Phase total_phase = Phase::zero();
                std::vector<VertexId> leaves_rem;
                std::vector<VertexId> axels_rem;

                for (size_t idx = 0; idx < g_list.size(); ++idx) {
                    VertexId lf = g_list[idx].first;
                    VertexId ax = g_list[idx].second;
                    Phase p = g.vertices[lf].phase;
                    if (g.vertices[ax].phase.is_pauli()) {
                        p = -p;
                    }
                    total_phase += p;

                    if (idx > 0) {
                        leaves_rem.push_back(lf);
                        if (ax != axel_keep) {
                            axels_rem.push_back(ax);
                        }
                    }
                }

                matches.push_back({leaf_keep, axel_keep, total_phase, leaves_rem, axels_rem});
            }
        }

        return matches;
    }

    static bool apply_fuse(Graph& g, const GadgetFusionMatch& m) {
        if (!g.is_valid_vertex(m.leaf_keep) || !g.is_valid_vertex(m.axel_keep)) return false;

        g.vertices[m.leaf_keep].phase = m.new_phase;
        g.vertices[m.axel_keep].phase = Phase::zero();

        for (VertexId lf : m.leaves_remove) {
            if (g.is_valid_vertex(lf)) {
                g.remove_vertex(lf);
            }
        }
        for (VertexId ax : m.axels_remove) {
            if (g.is_valid_vertex(ax)) {
                g.remove_vertex(ax);
            }
        }

        // If total phase became 0 or Clifford, it can be reduced further
        if (g.vertices[m.leaf_keep].phase.is_zero()) {
            g.remove_vertex(m.leaf_keep);
            if (g.is_valid_vertex(m.axel_keep) && g.degree(m.axel_keep) == 0) {
                g.remove_vertex(m.axel_keep);
            }
        }

        return true;
    }
};

} // namespace zx
