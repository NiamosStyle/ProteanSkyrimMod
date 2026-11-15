def wrist_shots():
    """The drawn wrist-blade from a 1st-person-like eye (behind and above the elbow) and from the side."""
    shoulder, elbow, wrist, a, up, side = arm_frame()
    shots_ = []
    show = ["MaleUnderwearBody:0", "HandMaleBig3rd", "TendrilFlesh", "TSinew_drawn", "TBone_drawn"]
    tgt = wrist + a * 8
    shots_.append(look("tendril_wrist_1st", show, tgt, elbow - a * 30 + up * 14 + side * 4))
    shots_.append(look("tendril_wrist_side", show, tgt, tgt + side * 60 + up * 5))
    shots_.append(look("tendril_wrist_under", show, tgt, tgt - up * 55 + side * 10))
    return shots_


# Measurements of the right hand in the arm's frame (for placing the wrist harpoon).
def probe():
    h = bones()
    shoulder, elbow, wrist, a, up, side = arm_frame()
    centre, n, along = palm_frame()
    out = {"a": tuple(round(x, 2) for x in a), "n": tuple(round(x, 2) for x in n), "along.a": round(along.dot(a), 3),
           "forearm_len": round((wrist - elbow).length, 1)}
    arm = bpy.data.objects["MaleBody_1.nif:ARMATURE"]
    for b in arm.data.bones:
        if b.name.startswith("NPC R Finger") or b.name == HAND:
            p = h(b.name) - wrist
            out[b.name[6:]] = (round(p.dot(a), 1), round(p.dot(n), 1), round(p.dot(a.cross(n).normalized()), 1))
    # hand mesh extent in the same frame
    hand = bpy.data.objects["HandMaleBig3rd"]
    c = a.cross(n).normalized()
    pts = [hand.matrix_world @ v.co - wrist for v in hand.data.vertices]
    pts = [p for p in pts if p.dot(a) > -2]
    out["hand_a"] = (round(min(p.dot(a) for p in pts), 1), round(max(p.dot(a) for p in pts), 1))
    out["hand_n"] = (round(min(p.dot(n) for p in pts), 1), round(max(p.dot(n) for p in pts), 1))
    out["hand_c"] = (round(min(p.dot(c) for p in pts), 1), round(max(p.dot(c) for p in pts), 1))
    return out
