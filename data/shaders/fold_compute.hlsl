// KHANARY fold_compute.hlsl
// Semantic fold kernel: elementwise weighted sum across fold-aligned semantic channels.
// [numthreads(64, 1, 1)] — sized for Intel HD 4600.
//
// Boundary:
//   fold_compute = semantic neighborhood/fold computation
//   attention    = semantic candidate weighting/admission
//
// This kernel combines two fold-compatible semantic buffers under arc_weight.
// It does not perform policy gating or XCFE authorization.

cbuffer FoldConstants : register(b0)
{
    uint  seq_len;     // number of semantic records
    uint  d_model;     // channel dimension per record
    float arc_weight;  // cos(phase_j - phase_i) — precomputed by CPU
    uint  pad;
};

StructuredBuffer<float>   node_a  : register(t0);  // semantic buffer A: [seq_len * d_model]
StructuredBuffer<float>   node_b  : register(t1);  // semantic buffer B: [seq_len * d_model]
RWStructuredBuffer<float> result  : register(u0);  // [seq_len * d_model]

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint idx = tid.x;
    if (idx >= seq_len * d_model) return;

    // Horizontal fold (same-phase): weighted elementwise sum
    // arc_weight = 1.0 for same-phase (Δθ = 0)
    float a = node_a[idx];
    float b = node_b[idx];
    result[idx] = arc_weight * (a + b);
}
