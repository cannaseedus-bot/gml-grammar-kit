// KHANARY attention.hlsl
// Semantic attention kernel (Yax execution backend), not transformer QKV.
//
// Boundary:
//   Bot Semantics -> Shader Attention -> Attended Semantics
//
// Score law:
//   S_i = W_i * C_i * R_i
//
// Policy law (hard gate, not score shaping):
//   legal_i = capable_i AND policy_ok_i AND xcfe_ok_i
//
// Admission:
//   A_i = 1 when semantic_score_i >= threshold and legal_i == 1
//       = 0 otherwise

cbuffer SemanticAttentionConstants : register(b0)
{
    uint  record_count;
    float admission_threshold;
    float relation_mix;  // [0,1]: blend relation/fold/phase modifiers into S
    float pad0;
};

struct SemanticRecord
{
    float W;   // static/pattern-strength channel
    float C;   // confidence channel
    float R;   // relevance channel
    float relation_weight; // neighborhood/relation weighting

    float fold_weight;     // fold compatibility weighting
    float phase_weight;    // phase compatibility weighting
    float policy_ok;       // 1.0 if policy requirement satisfied, else 0.0
    float capable;         // 1.0 if resolver has required capabilities, else 0.0

    float xcfe_ok;         // 1.0 if XCFE authorization holds, else 0.0
    float reserved0;
    float reserved1;
    float reserved2;
};

StructuredBuffer<SemanticRecord> Records : register(t0);

// BaseScore: raw S_i = W*C*R before relation/fold/phase modifiers.
RWStructuredBuffer<float> BaseScore : register(u0);
// SemanticScore: gated/weighted score used for ranking within legal candidates.
RWStructuredBuffer<float> SemanticScore : register(u1);
// LegalMask: 1 if capability+policy+XCFE all satisfied, else 0.
RWStructuredBuffer<uint> LegalMask : register(u2);
// AdmissionMask: 1 if admitted by threshold and legal gate, else 0.
RWStructuredBuffer<uint> AdmissionMask : register(u3);

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint idx = tid.x;
    if (idx >= record_count) return;

    SemanticRecord rec = Records[idx];

    float w = saturate(rec.W);
    float c = saturate(rec.C);
    float r = saturate(rec.R);
    float rel = saturate(rec.relation_weight);
    float fold = saturate(rec.fold_weight);
    float phase = saturate(rec.phase_weight);

    float s = w * c * r;
    BaseScore[idx] = s;

    // Semantic weighting layer (still symbolic channels, not token tensors).
    float semantic_modifier = lerp(1.0f, rel * fold * phase, saturate(relation_mix));
    float weighted = s * semantic_modifier;

    // Hard legality gate: policy constrains eligibility, not ranking algebra.
    uint legal = (rec.policy_ok >= 0.5f && rec.capable >= 0.5f && rec.xcfe_ok >= 0.5f) ? 1u : 0u;
    LegalMask[idx] = legal;

    float score = (legal == 1u) ? weighted : 0.0f;
    SemanticScore[idx] = score;
    AdmissionMask[idx] = (legal == 1u && score >= admission_threshold) ? 1u : 0u;
}
