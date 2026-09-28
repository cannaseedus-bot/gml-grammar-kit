graph [
    comment "KHANARY cognitive topology — 8 core tracks mapped to Q3 hypercube. Bit0: analysis(0)/generation(1). Bit1: active(0)/memory-grounded(1). Bit2: object-level(0)/meta-orchestration(1). Hamming-1 edges = semantically adjacent tracks. Nodes with registered track labels bind directly; no phantom nodes created."
    directed 0
    id 100
    label "KHANARY_TOPOLOGY"
    node [ id 0 label "REASON-µ"    x -1.0 y -1.0 z 0.0 ]
    node [ id 1 label "CREATE-µ"    x  1.0 y -1.0 z 0.0 ]
    node [ id 2 label "INFER-µ"     x -1.0 y  1.0 z 0.0 ]
    node [ id 3 label "ELIZA"       x  1.0 y  1.0 z 0.0 ]
    node [ id 4 label "TOOL-µ"      x -1.0 y -1.0 z 1.0 ]
    node [ id 5 label "ORCH-µ"      x  1.0 y -1.0 z 1.0 ]
    node [ id 6 label "MEM-µ"       x -1.0 y  1.0 z 1.0 ]
    node [ id 7 label "Coordinator" x  1.0 y  1.0 z 1.0 ]
    edge [ source 0 target 1 label "bit0:analysis-generation" weight 1.0  verb "@related_to" ]
    edge [ source 2 target 3 label "bit0:analysis-generation" weight 1.0  verb "@related_to" ]
    edge [ source 4 target 5 label "bit0:analysis-generation" weight 1.0  verb "@related_to" ]
    edge [ source 6 target 7 label "bit0:analysis-generation" weight 1.0  verb "@related_to" ]
    edge [ source 0 target 2 label "bit1:active-memory"       weight 1.0  verb "@related_to" ]
    edge [ source 1 target 3 label "bit1:active-memory"       weight 1.0  verb "@related_to" ]
    edge [ source 4 target 6 label "bit1:active-memory"       weight 1.0  verb "@related_to" ]
    edge [ source 5 target 7 label "bit1:active-memory"       weight 1.0  verb "@related_to" ]
    edge [ source 0 target 4 label "bit2:object-meta"         weight 1.0  verb "@related_to" ]
    edge [ source 1 target 5 label "bit2:object-meta"         weight 1.0  verb "@related_to" ]
    edge [ source 2 target 6 label "bit2:object-meta"         weight 1.0  verb "@related_to" ]
    edge [ source 3 target 7 label "bit2:object-meta"         weight 1.0  verb "@related_to" ]
]
