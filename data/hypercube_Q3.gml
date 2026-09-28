graph [
    comment "Hypercube Q3 — 8 vertices (3-bit binary labels), 12 edges (Hamming distance 1). Undirected, 3-regular."
    directed 0
    id 3
    label "Q3"
    node [
        id 0
        label "000"
        x -1.0
        y -1.0
        z 0.0
    ]
    node [
        id 1
        label "001"
        x 1.0
        y -1.0
        z 0.0
    ]
    node [
        id 2
        label "010"
        x -1.0
        y 1.0
        z 0.0
    ]
    node [
        id 3
        label "011"
        x 1.0
        y 1.0
        z 0.0
    ]
    node [
        id 4
        label "100"
        x -1.0
        y -1.0
        z 1.0
    ]
    node [
        id 5
        label "101"
        x 1.0
        y -1.0
        z 1.0
    ]
    node [
        id 6
        label "110"
        x -1.0
        y 1.0
        z 1.0
    ]
    node [
        id 7
        label "111"
        x 1.0
        y 1.0
        z 1.0
    ]
    edge [
        source 0
        target 1
        label "bit0"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 0
        target 2
        label "bit1"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 0
        target 4
        label "bit2"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 1
        target 3
        label "bit1"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 1
        target 5
        label "bit2"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 2
        target 3
        label "bit0"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 2
        target 6
        label "bit2"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 3
        target 7
        label "bit2"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 4
        target 5
        label "bit0"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 4
        target 6
        label "bit1"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 5
        target 7
        label "bit1"
        weight 1.0
        verb "@related_to"
    ]
    edge [
        source 6
        target 7
        label "bit0"
        weight 1.0
        verb "@related_to"
    ]
]
