"""GML semantic validator conformance suite.

Validates the rules declared in data/grammar/gml.validator.json against
fixture vectors (data/conformance/gml_validator.json) and all shipped GML
files (data/*.gml).

Suite name: gml_validator  (must match run_conformance.py required set)
"""
import json
import math
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from conformance_common import verdict

ROOT = Path(__file__).resolve().parents[1]

# Canonical verbs from src/micronaut/relation_isa.h (GML subset)
CANONICAL_VERBS = frozenset([
    "@related_to", "@delegates_to", "@requires", "@part_of", "@defines",
    "@is_a", "@causes", "@contradicts", "@has_part", "@has_property",
    "@possesses", "@value", "@imports", "@calls", "@exposes",
])

# ── Minimal GML tokenizer ─────────────────────────────────────────────────────

_TOKEN_RE = re.compile(
    r'"(?:[^"\\]|\\.)*"'                      # quoted string
    r'|[-+]?\d+\.\d*(?:[eE][-+]?\d+)?'       # float with decimal point
    r'|[-+]?\d+[eE][-+]?\d+'                  # float with exponent only
    r'|[-+]?\d+'                               # integer
    r'|[A-Za-z_][A-Za-z0-9_]*'               # identifier
    r'|[\[\]]',                                # brackets
    re.ASCII
)


def _tokenize(s):
    return _TOKEN_RE.findall(s)


def _parse_value(tok):
    if tok.startswith('"'):
        return tok[1:-1].replace('\\"', '"')
    try:
        return int(tok)
    except ValueError:
        pass
    try:
        return float(tok)
    except ValueError:
        pass
    return tok


def _parse_block(toks, i):
    """Return (pairs_list, i_after_closing_bracket).
    i points to the first token inside the block (after '[')."""
    pairs = []
    while i < len(toks) and toks[i] != ']':
        key = toks[i]
        i += 1
        if i >= len(toks):
            break
        if toks[i] == '[':
            i += 1
            sub, i = _parse_block(toks, i)
            if i < len(toks) and toks[i] == ']':
                i += 1
            val = sub  # list of (key, value) pairs → converted below
        else:
            val = _parse_value(toks[i])
            i += 1
        pairs.append((key, val))
    return pairs, i


def _pairs_to_dict(pairs):
    d = {}
    for k, v in pairs:
        if isinstance(v, list):
            v = _pairs_to_dict(v)
        if k in d:
            existing = d[k]
            if not isinstance(existing, list):
                existing = d[k] = [existing]
            existing.append(v)
        else:
            d[k] = v
    return d


def _parse_gml(text):
    toks = _tokenize(text)
    if not toks or toks[0] != 'graph':
        raise ValueError("expected 'graph'")
    if len(toks) < 2 or toks[1] != '[':
        raise ValueError("expected '[' after graph")
    pairs, _ = _parse_block(toks, 2)
    g = _pairs_to_dict(pairs)
    nodes = g.pop('node', [])
    if isinstance(nodes, dict):
        nodes = [nodes]
    edges = g.pop('edge', [])
    if isinstance(edges, dict):
        edges = [edges]
    return {'attrs': g, 'nodes': nodes, 'edges': edges}


# ── Stem derivation ───────────────────────────────────────────────────────────

def _derive_stem(filename):
    raw = re.sub(r'[^A-Za-z0-9]', '_', Path(filename).stem.lower())
    return raw


# ── Edit distance for near-miss detection ────────────────────────────────────

def _edit_dist(a, b):
    m, n = len(a), len(b)
    row = list(range(n + 1))
    for i in range(1, m + 1):
        prev, row[0] = row[0], i
        for j in range(1, n + 1):
            prev, row[j] = row[j], min(
                row[j] + 1, row[j - 1] + 1,
                prev + (0 if a[i - 1] == b[j - 1] else 1)
            )
    return row[n]


def _is_near_miss(verb):
    return any(_edit_dist(verb, cv) <= 2 for cv in CANONICAL_VERBS)


# ── Validator ─────────────────────────────────────────────────────────────────

def _check_range(value, lo, hi):
    return isinstance(value, (int, float)) and lo <= float(value) <= hi


def validate_gml(gml_text, stem='__default__'):
    """Return {'grade': VALID|PARSED, 'errors': [...], 'warnings': [...]}."""
    errors = []
    warnings = []

    try:
        graph = _parse_gml(gml_text)
    except Exception:
        return {'grade': 'PARSED', 'errors': ['PARSE_ERROR'], 'warnings': []}

    nodes = graph['nodes']
    edges = graph['edges']

    # ── V1: node id required and unique ──────────────────────────────────────
    seen_ids = set()
    for node in nodes:
        nid = node.get('id')
        if nid is None:
            if 'MISSING_NODE_ID' not in errors:
                errors.append('MISSING_NODE_ID')
        elif nid in seen_ids:
            if 'DUPLICATE_NODE_ID' not in errors:
                errors.append('DUPLICATE_NODE_ID')
        else:
            seen_ids.add(nid)

    # ── V2/V3: edge endpoints required and resolvable ─────────────────────────
    for edge in edges:
        src = edge.get('source')
        tgt = edge.get('target')
        if src is None or tgt is None:
            if 'MISSING_EDGE_ENDPOINT' not in errors:
                errors.append('MISSING_EDGE_ENDPOINT')
        else:
            if src not in seen_ids or tgt not in seen_ids:
                if 'INVALID_EDGE_ENDPOINT' not in errors:
                    errors.append('INVALID_EDGE_ENDPOINT')

    # ── V5: file-stem ident safety ────────────────────────────────────────────
    if stem and stem[0].isdigit():
        if 'STEM_STARTS_WITH_DIGIT' not in errors:
            errors.append('STEM_STARTS_WITH_DIGIT')

    # ── R1–R5: relation authority ─────────────────────────────────────────────
    for edge in edges:
        verb = edge.get('verb')
        relation = edge.get('relation')

        if verb is not None and relation is not None:
            if 'BOTH_VERB_AND_RELATION_PRESENT' not in warnings:
                warnings.append('BOTH_VERB_AND_RELATION_PRESENT')

        # effective verb: verb wins; relation is fallback candidate
        eff = verb if verb is not None else relation
        if verb is None and relation is not None:
            if 'RELATION_ALIAS_USED' not in warnings:
                warnings.append('RELATION_ALIAS_USED')

        if eff is not None and eff not in CANONICAL_VERBS:
            if 'INFER_CANDIDATE_USED' not in warnings:
                warnings.append('INFER_CANDIDATE_USED')
            if _is_near_miss(eff):
                if 'NEAR_MISS_VERB' not in warnings:
                    warnings.append('NEAR_MISS_VERB')

    # ── W1–W4: WCR unit-float ranges ─────────────────────────────────────────
    for node in nodes:
        if not _check_range(node.get('confidence', 0.5), 0.0, 1.0):
            if node.get('confidence') is not None and 'CONFIDENCE_OUT_OF_RANGE' not in errors:
                errors.append('CONFIDENCE_OUT_OF_RANGE')
        if not _check_range(node.get('static_weight', 0.5), 0.0, 1.0):
            if node.get('static_weight') is not None and 'STATIC_WEIGHT_OUT_OF_RANGE' not in errors:
                errors.append('STATIC_WEIGHT_OUT_OF_RANGE')
        if not _check_range(node.get('health', 0.5), 0.0, 1.0):
            if node.get('health') is not None and 'HEALTH_OUT_OF_RANGE' not in errors:
                errors.append('HEALTH_OUT_OF_RANGE')

    for edge in edges:
        w = edge.get('weight')
        if w is not None and not _check_range(w, 0.0, 1.0):
            if 'EDGE_WEIGHT_OUT_OF_RANGE' not in errors:
                errors.append('EDGE_WEIGHT_OUT_OF_RANGE')

    # ── G1: finite coordinates ────────────────────────────────────────────────
    for node in nodes:
        for coord in ('x', 'y', 'z'):
            v = node.get(coord)
            if v is not None and isinstance(v, float) and not math.isfinite(v):
                if 'COORDINATE_NOT_FINITE' not in errors:
                    errors.append('COORDINATE_NOT_FINITE')

    # ── G2: partial coordinate vector ─────────────────────────────────────────
    for node in nodes:
        present = sum(1 for c in ('x', 'y', 'z') if node.get(c) is not None)
        if 0 < present < 3:
            if 'PARTIAL_COORDINATE_VECTOR' not in warnings:
                warnings.append('PARTIAL_COORDINATE_VECTOR')

    grade = 'PARSED' if errors else 'VALID'
    return {'grade': grade, 'errors': errors, 'warnings': warnings}


# ── Case checker ─────────────────────────────────────────────────────────────

def _check(result, expected_grade, expected_errors, expected_warnings):
    if result['grade'] != expected_grade:
        return False
    for e in expected_errors:
        if e not in result['errors']:
            return False
    for w in expected_warnings:
        if w not in result['warnings']:
            return False
    # Clean cases: no unexpected issues
    if not expected_errors and not expected_warnings and expected_grade == 'VALID':
        if result['errors'] or result['warnings']:
            return False
    return True


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    cases = []
    try:
        fixture_path = ROOT / 'data/conformance/gml_validator.json'
        fixtures = json.loads(fixture_path.read_text(encoding='utf-8'))

        for row in fixtures:
            case_id = row['id']
            stem = row.get('stem', _derive_stem(case_id + '.gml'))
            result = validate_gml(row['gml'], stem)
            passed = _check(result, row['grade'], row['errors'], row['warnings'])
            cases.append((case_id, passed))

        # Shipped GML files — all must be grade VALID with no errors
        for gml_path in sorted((ROOT / 'data').glob('*.gml')):
            stem = _derive_stem(gml_path.name)
            gml_text = gml_path.read_text(encoding='utf-8', errors='replace')
            result = validate_gml(gml_text, stem)
            passed = result['grade'] == 'VALID' and not result['errors']
            cases.append(('shipped/' + gml_path.name, passed))

        return verdict('gml_validator', cases)
    except Exception as exc:
        return verdict('gml_validator', cases, str(exc))


if __name__ == '__main__':
    raise SystemExit(main())
