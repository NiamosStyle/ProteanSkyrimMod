"""The 1st-person sleeves are the 3rd-person ones cut at the elbow (the upper arm would fill the 1st-person camera).
Cut by bone weights, the edge came out ragged and open: during a Hammerfist swing the camera looked straight into the
open end of the sleeve and its loose flaps (user, screenshot 92, 2026-11-13). cut() now trims the flaps and closes
each opening the cut made with a shallow cap that dips into the arm, painted with the flesh beside it.

  cut(d, keep) -> a new shape dict (verts, normals, uvs, tris, weights) for tools/build_*.py's add_shape
"""
import math


def _key(p):
    return (round(p[0], 3), round(p[1], 3), round(p[2], 3))


def _edges(tris, pk):
    """Edges by position (the texture seams split points; they must not count as openings) -> triangle count."""
    count = {}
    for t in tris:
        for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            e = frozenset((pk[a], pk[b]))
            count[e] = count.get(e, 0) + 1
    return count


def cut(d, keep, trim=3, dip=0.35):
    verts, normals, uvs, weights = d["verts"], d["normals"], d["uvs"], d["weights"]
    pk = [_key(v) for v in verts]
    open_before = {e for e, n in _edges(d["tris"], pk).items() if n == 1}
    tris = [t for t in d["tris"] if all(keep(i) for i in t)]
    # loose flaps: triangles hanging on by one edge, peeled off a few times over
    for _ in range(trim):
        cnt = _edges(tris, pk)
        def loose(t):
            return sum(cnt[frozenset((pk[a], pk[b]))] == 1 for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0]))) >= 2
        tris = [t for t in tris if not loose(t)]
    cnt = _edges(tris, pk)
    # the openings the cut made (not the ones the shape had already), as directed edges in the triangles' order
    nxt = {}
    rep = {}   # a position -> one point index there (for its weights and texture place)
    for t in tris:
        for a, b in ((t[0], t[1]), (t[1], t[2]), (t[2], t[0])):
            e = frozenset((pk[a], pk[b]))
            if cnt[e] == 1 and e not in open_before:
                nxt[pk[a]] = pk[b]
                rep.setdefault(pk[a], a); rep.setdefault(pk[b], b)
    loops, seen = [], set()
    for start in list(nxt):
        if start in seen:
            continue
        loop, k = [], start
        while k in nxt and k not in seen:
            seen.add(k); loop.append(k); k = nxt[k]
        if len(loop) >= 3 and k == start:
            loops.append(loop)
    out = dict(verts=[list(v) for v in verts], normals=[list(n) for n in normals], uvs=[list(u) for u in uvs],
               weights=[dict(w) for w in weights], tris=[list(t) for t in tris])
    used = {i for t in tris for i in t}
    mid = [sum(verts[i][k] for i in used) / len(used) for k in range(3)]   # somewhere inside the sleeve
    for loop in loops:
        ids = [rep[k] for k in loop]
        c = [sum(verts[i][k] for i in ids) / len(ids) for k in range(3)]
        r = sum(math.dist(verts[i], c) for i in ids) / len(ids)
        inward = [mid[k] - c[k] for k in range(3)]
        L = math.sqrt(sum(x * x for x in inward)) or 1.0
        inward = [x / L for x in inward]
        center = [c[k] + inward[k] * r * dip for k in range(3)]
        w = {}
        for i in ids:
            for b, x in weights[i].items():
                w[b] = w.get(b, 0.0) + x
        top = sorted(w.items(), key=lambda x: -x[1])[:4]
        s = sum(x for _, x in top) or 1.0
        w = {b: x / s for b, x in top}
        n_out = [-x for x in inward]
        base = len(out["verts"])
        # the rim again, on the cap's side (its own shading), then the middle
        for i in ids:
            out["verts"].append(list(verts[i])); out["normals"].append(n_out); out["uvs"].append(list(uvs[i]))
            out["weights"].append(dict(weights[i]))
        out["verts"].append(center); out["normals"].append(n_out)
        out["uvs"].append([sum(uvs[i][0] for i in ids) / len(ids), sum(uvs[i][1] for i in ids) / len(ids)])
        out["weights"].append(w)
        ci = base + len(ids)
        # the loop runs the same way as the sleeve's own triangles along this edge, so the cap's triangles run it
        # backwards; check against the outward direction all the same
        a, b = 0, 1
        tri = [base + b, base + a, ci]
        p0, p1, p2 = (out["verts"][j] for j in tri)
        e1 = [p1[k] - p0[k] for k in range(3)]; e2 = [p2[k] - p0[k] for k in range(3)]
        nrm = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
        flip = sum(nrm[k] * n_out[k] for k in range(3)) < 0
        n = len(ids)
        for j in range(n):
            a, b = base + j, base + (j + 1) % n
            out["tris"].append([a, b, ci] if flip else [b, a, ci])
    return out, len(loops)
