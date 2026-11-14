# The pictures sent to the user: baked textures, brightened so the dark flesh can be read
import bpy
def final():
    sc = bpy.data.scenes["Claws"]; old = sc.view_settings.exposure
    sc.view_settings.exposure = 1.6
    try:
        a = textured('claws_male', {'RCFlesh': 'Arm', 'RCClaw': 'Claw', 'RCFleshL': 'Arm', 'RCClawL': 'Claw'}, MALE,
                     [('both', (0, 0, 86), (90, 0, 180), 150), ('hand', (30, 5, 62), (80, 0, 125), 42), ('side', (24, 0, 86), (90, 0, 90), 95)])
        b = textured('claws_female', {'RCFleshF': 'Arm', 'RCClawF': 'Claw', 'RCFleshFL': 'Arm', 'RCClawFL': 'Claw'},
                     ['FemaleUnderwearBody:0'], [('both', (0, 0, 86), (90, 0, 180), 150)])
    finally:
        sc.view_settings.exposure = old
    return a + b
