# K'UHUL GML Grammar Kit

Khanary GML grammar, semantic validator contract, conformance fixtures, and sample GML graphs for the KHANARY / K'UHUL semantic runtime stack.

This kit preserves the original KHANARY.CPP paths so the validator and related contract references work as-is.

## Files

| File | Description |
|------|-------------|
| `data/grammar/gml.khanary.ebnf` | Permissive GML grammar for Khanary semantic graph ingestion. |
| `data/grammar/gml.validator.json` | Semantic validator contract: structural, relation, WCR, geometry, and authority checks. |
| `data/conformance/gml_validator.json` | Positive and negative conformance fixtures for the validator. |
| `data/*.gml` | Shipped GML sample/specimen graphs. |
| `data/manifest/gml.manifest.json` | Manifest entry for the GML grammar surface. |
| `tools/check_gml_validator.py` | Dependency-free validator/conformance runner. |
| `tools/conformance_common.py` | Shared conformance verdict helper. |
| `skills/math-agent/SKILL.md` | Deterministic scalar, vector dot2, and 2x2 matrix skill metadata for grammar/vector consumers. |
| `src/world/gml_parser.*` | Native parser implementation reference. |
| `src/world/field_dag.*` | Field DAG overlay/projection reference. |
| `src/micronaut/relation_isa.*` | Canonical relation authority reference. |
| `src/kuhul/xul_gate.h` | Commit boundary reference named by the validator law. |
| `data/grammar/fold_graph.xml`, `fold.cd.xml`, `vectors.mml` | Companion fold/vector contracts referenced by the GML validator surface. |

## Validate

```powershell
python tools/check_gml_validator.py
```

Expected result: a JSON conformance verdict with `"verdict":"PASS"` for suite `gml_validator`.

## Part of

[KHANARY.CPP](https://github.com/cannaseedus-bot) — K'UHUL semantic runtime stack.
