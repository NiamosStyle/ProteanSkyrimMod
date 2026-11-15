def node_shot(tag):
    """The drawn hand seen from the front of the palm (as the camera sees a casting hand), with a red ball on the
    1st-person spell's starting point."""
    centre, n, along = palm_frame()
    ball = bpy.data.objects.get("MagicBall")
    if not ball:
        me = bpy.data.meshes.new("MagicBall")
        import bmesh as bm_
        b = bm_.new(); bm_.ops.create_uvsphere(b, u_segments=16, v_segments=8, radius=1.2); b.to_mesh(me); b.free()
        ball = bpy.data.objects.new("MagicBall", me)
        bpy.data.scenes[SCENE].collection.objects.link(ball)
        ball.data.materials.append(material("BallRed", (1, 0, 0, 1)))
    ball.location = MAGIC_1ST
    show = ["HandMaleBig3rd", "TendrilFlesh", "TSinew_drawn", "TBone_drawn", "MagicBall"]
    import math
    eye = MAGIC_1ST + (n * 1.0 + along * -0.6).normalized() * 70
    q = (MAGIC_1ST - eye).to_track_quat('-Z', 'Y')
    side = preview("node_%s_side" % tag, show, MAGIC_1ST, [math.degrees(x) for x in q.to_euler()], 70, color='MATERIAL')
    eye = MAGIC_1ST + along.cross(n).normalized() * 70
    q = (MAGIC_1ST - eye).to_track_quat('-Z', 'Y')
    prof = preview("node_%s_profile" % tag, show, MAGIC_1ST, [math.degrees(x) for x in q.to_euler()], 70, color='MATERIAL')
    return side, prof


# Where the drawn harpoon sits against the spell's starting point (the hand's MagicNode).
def probe():
    arm = bpy.data.objects["MaleBody_1.nif:ARMATURE"]
    names = [b.name for b in arm.data.bones if "Magic" in b.name]
    h = bones()
    centre, n, along = palm_frame()
    out = {"magic bones": names}
    for nm in names:
        if nm.startswith("NPC R"):
            p = h(nm) - h(HAND)
            out[nm] = {"along": round(p.dot(along), 2), "out_of_palm": round(p.dot(n), 2),
                       "across": round(p.dot(along.cross(n).normalized()), 2)}
    c = centre - h(HAND)
    out["palm centre"] = {"along": round(c.dot(along), 2), "out_of_palm": round(c.dot(n), 2)}
    return out
