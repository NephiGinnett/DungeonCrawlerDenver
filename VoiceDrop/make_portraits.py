"""Draw the Voice Drop cast portraits as SVG badges in public/portraits/<speaker id>.svg.

    python make_portraits.py

Every portrait shares one set of proportions (head, neck, shoulders inside a circle) and the
Voice Drop palette, so the cast reads as one set. Each character is a short recipe at the bottom:
skin, hair, hat, clothes, face and a prop. Add a new speaker by adding a recipe with its catalog id.
"""
from pathlib import Path

OUT = Path(__file__).resolve().parent / "public" / "portraits"
INK = "#1a1424"

SKIN = {"fair": ("#f3d2bd", "#d9a98f"), "light": ("#e9bf9c", "#cc9877"), "tan": ("#cf9a6c", "#ad7a50"),
        "medium": ("#b57c52", "#93603b"), "brown": ("#8d5a3b", "#6f4329"), "deep": ("#5e3a26", "#46291a")}


def shade(skin):
    return SKIN[skin]


# ---------------------------------------------------------------- parts
def background(c1, c2):
    return f'''<defs><radialGradient id="bg" cx="50%" cy="38%" r="70%"><stop offset="0" stop-color="{c1}"/><stop offset="1" stop-color="{c2}"/></radialGradient>
<clipPath id="clip"><circle cx="100" cy="100" r="98"/></clipPath></defs>
<circle cx="100" cy="100" r="98" fill="url(#bg)"/>'''


def body(shirt, skin, neck=True, collar=None):
    s, d = shade(skin)
    out = ""
    if neck:
        out += f'<path d="M86 118h28v26c0 6-28 6-28 0z" fill="{d}"/>'
    out += f'<path d="M22 204c2-34 22-54 52-60 8 6 44 6 52 0 30 6 50 26 52 60z" fill="{shirt}"/>'
    if collar:
        out += f'<path d="M80 142l20 20 20-20 6 4-26 26-26-26z" fill="{collar}"/>'
    return out


def head(skin, ears=True, jaw="M66 92c0 26 14 44 34 44s34-18 34-44"):
    s, d = shade(skin)
    out = ""
    if ears:
        out += f'<ellipse cx="66" cy="96" rx="7" ry="10" fill="{d}"/><ellipse cx="134" cy="96" rx="7" ry="10" fill="{d}"/>'
    out += f'<path d="M66 92c0-26 14-42 34-42s34 16 34 42c0 26-14 44-34 44s-34-18-34-44z" fill="{s}"/>'
    out += f'<path d="M118 60c10 8 16 20 16 32 0 26-14 44-34 44 14-8 22-24 22-44 0-12-2-22-4-32z" fill="{d}" opacity=".45"/>'
    return out


def eyes(kind="open", look=0):
    l, r = 87 + look, 113 + look
    if kind == "closed":      # serene, eyes shut in a smile
        return f'<path d="M{l-5} 94q5-5 10 0M{r-5} 94q5-5 10 0" stroke="{INK}" stroke-width="2.6" fill="none" stroke-linecap="round"/>'
    if kind == "wide":
        return (f'<ellipse cx="{l}" cy="92" rx="6" ry="7" fill="#fff"/><ellipse cx="{r}" cy="92" rx="6" ry="7" fill="#fff"/>'
                f'<circle cx="{l}" cy="93" r="3" fill="{INK}"/><circle cx="{r}" cy="93" r="3" fill="{INK}"/>')
    if kind == "squint":
        return f'<path d="M{l-5} 93h10M{r-5} 93h10" stroke="{INK}" stroke-width="3" stroke-linecap="round"/>'
    if kind == "glow":        # eyes shining out of a shadow
        return f'<ellipse cx="{l}" cy="94" rx="3.5" ry="2.5" fill="#43e6d0"/><ellipse cx="{r}" cy="94" rx="3.5" ry="2.5" fill="#43e6d0"/>'
    return (f'<ellipse cx="{l}" cy="93" rx="4" ry="5" fill="{INK}"/><ellipse cx="{r}" cy="93" rx="4" ry="5" fill="{INK}"/>'
            f'<circle cx="{l+1.5}" cy="91" r="1.4" fill="#fff"/><circle cx="{r+1.5}" cy="91" r="1.4" fill="#fff"/>')


def brows(kind="neutral", color=INK):
    p = {"neutral": "M80 83q7-4 14 0M106 83q7-4 14 0",
         "angry": "M79 80l15 6M121 80l-15 6",
         "raised": "M80 79q7-6 14-1M106 78q7-5 14 1",
         "kind": "M80 84q7-5 14-2M106 82q7-3 14 2",
         "stern": "M79 83l15 1M106 84l15-1",
         "worried": "M80 84l14-5M120 84l-14-5"}[kind]
    return f'<path d="{p}" stroke="{color}" stroke-width="3.4" fill="none" stroke-linecap="round"/>'


def nose(skin):
    return f'<path d="M100 96q-4 10 1 12" stroke="{shade(skin)[1]}" stroke-width="2.4" fill="none" stroke-linecap="round"/>'


def mouth(kind="smile"):
    return {
        "smile": f'<path d="M89 116q11 9 22 0" stroke="{INK}" stroke-width="3" fill="none" stroke-linecap="round"/>',
        "grin": f'<path d="M87 113q13 14 26 0z" fill="{INK}"/><path d="M90 114h20v3H90z" fill="#fff"/>',
        "gap": f'<path d="M86 113q14 14 28 0z" fill="{INK}"/><path d="M89 114h9v3h-9zM103 114h8v3h-8z" fill="#f3ead2"/>',
        "flat": f'<path d="M91 117h18" stroke="{INK}" stroke-width="3" stroke-linecap="round"/>',
        "frown": f'<path d="M90 120q10-7 20 0" stroke="{INK}" stroke-width="3" fill="none" stroke-linecap="round"/>',
        "shout": f'<ellipse cx="100" cy="118" rx="10" ry="8" fill="{INK}"/><path d="M92 113h16v3H92z" fill="#fff"/><ellipse cx="100" cy="122" rx="5" ry="2.5" fill="#e2566f"/>',
        "scream": f'<ellipse cx="100" cy="119" rx="15" ry="13" fill="{INK}"/><path d="M88 110h24v4H88z" fill="#fff"/><ellipse cx="100" cy="126" rx="8" ry="4" fill="#e2566f"/>',
        "o": f'<ellipse cx="100" cy="117" rx="4" ry="5" fill="{INK}"/>',
        "smirk": f'<path d="M90 117q10 3 20-4" stroke="{INK}" stroke-width="3" fill="none" stroke-linecap="round"/>',
        "talk": f'<path d="M90 114q10 10 20 0z" fill="{INK}"/>',
    }[kind]


def blush():
    return '<ellipse cx="80" cy="106" rx="6" ry="3.5" fill="#ff7aa8" opacity=".35"/><ellipse cx="120" cy="106" rx="6" ry="3.5" fill="#ff7aa8" opacity=".35"/>'


def freckles(color="#a8673f"):
    return "".join(f'<circle cx="{x}" cy="{y}" r="1.2" fill="{color}"/>' for x, y in
                   [(80, 103), (84, 106), (78, 107), (116, 103), (120, 106), (122, 102)])


def glasses(color="#2b2238", lens="#cfe9ff"):
    return (f'<g fill="{lens}" fill-opacity=".18" stroke="{color}" stroke-width="2.6"><rect x="77" y="85" width="20" height="15" rx="5"/>'
            f'<rect x="103" y="85" width="20" height="15" rx="5"/></g><path d="M97 91h6M77 90l-9-2M123 90l9-2" stroke="{color}" stroke-width="2.6"/>')


# ---------------------------------------------------------------- hair & hats
def hair(style, color):
    p = {
        "curly_big": "M58 98c-10-18-4-44 14-54 10-14 34-16 46-6 18 2 30 18 28 38 8 12 2 28-4 30 0-16-4-30-10-38-14 4-30 4-44-2-12 8-18 20-18 32-6 4-10 4-12 0z",
        "bob": "M64 96c-4-28 12-48 36-48s40 20 36 48c-2 8-6 12-8 12 2-14 0-28-8-36-12 6-30 8-48 2-6 8-8 20-6 34-2 0-2-4-2-12z",
        "short": "M66 84c0-22 14-36 34-36s34 14 34 36c-6-10-16-16-34-16s-28 6-34 16z",
        "bun": "M66 88c0-24 14-38 34-38s34 14 34 38c-8-12-18-18-34-18s-26 6-34 18z",
        "ponytail": "M66 88c0-24 14-38 34-38s34 14 34 38c-8-12-18-18-34-18s-26 6-34 18zM132 70c16 6 22 26 16 46-4-12-10-20-18-24z",
        "spiky": "M64 84l-4-22 14 8 2-20 12 14 8-20 8 18 10-16 4 20 14-8-4 22c-10-10-22-14-34-14s-24 4-30 18z",
        "braids": "M66 88c0-24 14-38 34-38s34 14 34 38c-8-12-18-18-34-18s-26 6-34 18z",
        "slick": "M66 86c0-24 14-38 34-38s34 14 34 38c-4-14-10-18-16-20-6 4-26 6-40 4-6 4-10 10-12 16z",
    }[style]
    out = f'<path d="{p}" fill="{color}"/>'
    if style == "bun":
        out += f'<circle cx="100" cy="46" r="13" fill="{color}"/>'
    if style == "braids":
        for x in (66, 134):
            out += "".join(f'<ellipse cx="{x}" cy="{y}" rx="6" ry="7" fill="{color}"/>' for y in range(104, 150, 11))
    return out


def hardhat(color, logo=None):
    out = (f'<path d="M62 80c0-24 16-38 38-38s38 14 38 38z" fill="{color}"/>'
           f'<path d="M100 42c-6 10-6 26-6 38h12c0-12 0-28-6-38z" fill="#000" opacity=".12"/>'
           f'<rect x="54" y="76" width="92" height="9" rx="4.5" fill="{color}"/><rect x="54" y="80" width="92" height="5" rx="2.5" fill="#000" opacity=".18"/>')
    if logo:
        out += f'<text x="100" y="72" text-anchor="middle" font-family="Verdana,sans-serif" font-size="9" font-weight="bold" fill="{INK}">{logo}</text>'
    return out


def cowboy(color, band="#3a2516"):
    return (f'<path d="M70 72c0-22 6-32 14-32 6 0 10 6 16 6s10-6 16-6c8 0 14 10 14 32z" fill="{color}"/>'
            f'<path d="M70 66h60v8H70z" fill="{band}"/>'
            f'<path d="M34 76c10 8 38 10 66 10s56-2 66-10c-4 12-30 18-66 18s-62-6-66-18z" fill="{color}"/>'
            f'<path d="M100 46v20" stroke="#000" stroke-opacity=".15" stroke-width="4"/>')


def mining_helmet(color="#d8b45a"):
    return (f'<path d="M64 82c0-24 16-38 36-38s36 14 36 38z" fill="{color}"/>'
            f'<rect x="58" y="78" width="84" height="8" rx="4" fill="{color}"/><rect x="58" y="82" width="84" height="4" rx="2" fill="#000" opacity=".2"/>'
            f'<rect x="90" y="54" width="20" height="16" rx="4" fill="#3a3346"/><circle cx="100" cy="62" r="6" fill="#6b6478"/>'
            f'<circle cx="100" cy="62" r="3" fill="#9c95a8"/>')


def prospector_hat(color="#7a5a3a"):
    return (f'<path d="M66 76c-2-20 10-34 34-34s38 12 34 34z" fill="{color}"/>'
            f'<path d="M78 50q6 6 2 14" stroke="#000" stroke-opacity=".2" stroke-width="2" fill="none"/>'
            f'<path d="M44 82c14-8 34-10 56-10s44 2 58 12c-14 2-20 0-26-2-20-4-48-4-68 0-6 2-12 2-20 0z" fill="{color}"/>'
            f'<circle cx="118" cy="56" r="3" fill="#000" opacity=".35"/>')


def ballcap(color, bill=None):
    bill = bill or color
    return (f'<path d="M66 82c0-24 14-36 34-36s34 12 34 36z" fill="{color}"/>'
            f'<path d="M66 80c-14 0-22 6-22 10h56c-6-6-18-10-34-10z" fill="{bill}"/><circle cx="100" cy="47" r="3" fill="{bill}"/>')


def beanie(color, stripe="#fff"):
    return (f'<path d="M64 84c0-26 14-40 36-40s36 14 36 40z" fill="{color}"/>'
            f'<rect x="62" y="76" width="76" height="12" rx="5" fill="{color}"/><rect x="62" y="80" width="76" height="3" fill="{stripe}" opacity=".5"/>')


def hood(color, inner="#0b0912"):
    return (f'<path d="M50 200c0-40 4-70 8-100 4-36 20-58 42-58s38 22 42 58c4 30 8 60 8 100z" fill="{color}"/>'
            f'<path d="M68 98c0-26 12-44 32-44s32 18 32 44c0 20-10 38-32 38s-32-18-32-38z" fill="{inner}"/>')


# ---------------------------------------------------------------- clothes & props
def hivis(color="#ff8a2a", stripe="#e8eef2"):
    return (f'<path d="M30 204c4-30 20-48 44-56l8 56zM170 204c-4-30-20-48-44-56l-8 56z" fill="{color}"/>'
            f'<path d="M34 184l48 4v8l-49-3zM166 184l-48 4v8l49-3z" fill="{stripe}" opacity=".9"/>')


def headset(color="#2b2238", mic="#ff4fa3"):
    return (f'<path d="M62 92c0-34 16-50 38-50s38 16 38 50" stroke="{color}" stroke-width="6" fill="none"/>'
            f'<rect x="54" y="84" width="14" height="22" rx="6" fill="{color}"/><rect x="132" y="84" width="14" height="22" rx="6" fill="{color}"/>'
            f'<path d="M60 104q4 18 26 16" stroke="{color}" stroke-width="3.5" fill="none"/><circle cx="88" cy="120" r="4.5" fill="{mic}"/>')


def star_badge(x=72, y=170, color="#d8b45a"):
    return (f'<path d="M{x} {y-11}l3.4 7 7.6 1-5.5 5.4 1.3 7.6-6.8-3.6-6.8 3.6 1.3-7.6-5.5-5.4 7.6-1z" fill="{color}" stroke="#8a6a1f" stroke-width="1.4"/>')


def mustache(color, big=False):
    if big:
        return f'<path d="M100 108c-8-4-20-4-28 6 10-2 18 2 28 0 10 2 18-2 28 0-8-10-20-10-28-6z" fill="{color}"/>'
    return f'<path d="M100 109c-6-3-14-3-18 3 6 0 12 1 18 0 6 1 12 0 18 0-4-6-12-6-18-3z" fill="{color}"/>'


def beard(color):
    return (f'<path d="M68 100c2 30 14 54 32 60 18-6 30-30 32-60-6 8-10 12-16 12-6-4-26-4-32 0-6 0-10-4-16-12z" fill="{color}"/>'
            '')


def pickaxe():
    return ('<path d="M146 200l20-90" stroke="#7a5a3a" stroke-width="7" stroke-linecap="round"/>'
            '<path d="M136 112q30-22 54 4-28-8-54-4z" fill="#9aa3ad" stroke="#5b636c" stroke-width="2"/>')


def banjo():
    return ('<path d="M150 210l14-104" stroke="#6b4426" stroke-width="8" stroke-linecap="round"/>'
            '<rect x="157" y="92" width="14" height="20" rx="3" fill="#4a2e18"/>'
            '<path d="M160 96h8M160 101h8M160 106h8" stroke="#d8b45a" stroke-width="1.5"/>'
            '<circle cx="146" cy="196" r="22" fill="#efe6d2" stroke="#8a6a1f" stroke-width="4"/>')


def stop_paddle():
    return ('<path d="M162 200v-66" stroke="#555" stroke-width="5"/>'
            '<path d="M150 104l12-6 12 6v14l-12 6-12-6z" fill="#e23b3b" stroke="#fff" stroke-width="2"/>'
            '<text x="162" y="115" text-anchor="middle" font-family="Verdana,sans-serif" font-size="7" font-weight="bold" fill="#fff">STOP</text>')


def energy_can():
    return ('<rect x="138" y="132" width="22" height="40" rx="4" fill="#8cff3a" transform="rotate(14 149 152)"/>'
            '<rect x="138" y="138" width="22" height="7" fill="#ff4fa3" transform="rotate(14 149 152)"/>'
            '<text x="149" y="160" text-anchor="middle" font-family="Verdana,sans-serif" font-size="6.5" font-weight="bold" fill="#0d0b14" transform="rotate(-76 149 160)">LIQUID</text>'
            '<path d="M162 124l6-8M168 132l9-3M152 120l1-9" stroke="#8cff3a" stroke-width="3" stroke-linecap="round"/>')


def phone(x=56, y=124, color="#2b2238"):
    return (f'<rect x="{x}" y="{y}" width="18" height="32" rx="4" fill="{color}" transform="rotate(-18 {x+9} {y+16})"/>'
            f'<rect x="{x+3}" y="{y+4}" width="12" height="20" rx="2" fill="#43e6d0" opacity=".8" transform="rotate(-18 {x+9} {y+16})"/>')


def receipt():
    return ('<path d="M140 128l26 6-8 40-26-6z" fill="#f7f3e8"/>'
            '<path d="M142 138l18 4M141 145l17 4M139 152l18 4M138 159l12 3" stroke="#9a92a8" stroke-width="1.6"/>')


def pencil():
    return '<path d="M128 70l16-22" stroke="#f2c14e" stroke-width="5" stroke-linecap="round"/><path d="M144 48l3-4" stroke="#333" stroke-width="5" stroke-linecap="round"/>'


def name_tag(text, x=118, y=166):
    return (f'<rect x="{x}" y="{y}" width="34" height="13" rx="2" fill="#f7f3e8"/>'
            f'<text x="{x+17}" y="{y+9.5}" text-anchor="middle" font-family="Verdana,sans-serif" font-size="7" font-weight="bold" fill="{INK}">{text}</text>')


def wind():
    return ('<g stroke="#a7c8ff" stroke-opacity=".35" stroke-width="3" fill="none" stroke-linecap="round">'
            '<path d="M18 70q20-8 36 0t30 0"/><path d="M130 50q18-8 34 0t22 0"/><path d="M150 150q16-6 30 0"/></g>')


def sparkles(color="#fff"):
    return "".join(f'<path d="M{x} {y-6}l1.6 4.4 4.4 1.6-4.4 1.6-1.6 4.4-1.6-4.4-4.4-1.6 4.4-1.6z" fill="{color}" opacity=".8"/>'
                   for x, y in [(36, 60), (164, 70), (150, 40), (44, 140)])


def lamp_glow():
    return '<path d="M100 62l-60-40h120z" fill="#fff6c2" opacity=".12"/>'


def earrings(color="#d8b45a"):
    return f'<circle cx="64" cy="110" r="4" fill="none" stroke="{color}" stroke-width="2.4"/><circle cx="136" cy="110" r="4" fill="none" stroke="{color}" stroke-width="2.4"/>'


def svg(name, *parts):
    inner = "".join(parts)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200" role="img" aria-label="{name}">'
            f'{parts[0]}<g clip-path="url(#clip)">{"".join(parts[1:])}</g>'
            f'<circle cx="100" cy="100" r="97" fill="none" stroke="#ff4fa3" stroke-opacity=".55" stroke-width="3"/></svg>')


def face(skin, e="open", b="neutral", m="smile", bc=INK, extra=""):
    return head(skin) + eyes(e) + brows(b, bc) + nose(skin) + mouth(m) + extra


# ---------------------------------------------------------------- the cast
CAST = {
    "Dispatch": ("Dispatch (Vonda)", lambda: svg("Dispatch (Vonda)",
        background("#ffb347", "#3a1d0c"),
        hair("curly_big", "#3b2216"), body("#7a4a8c", "brown", collar="#efe9f7"),
        face("brown", "open", "raised", "talk", extra=glasses("#3a1d0c")), headset())),
    "Jan": ("Clerk Jan", lambda: svg("Clerk Jan",
        background("#43e6d0", "#0d2a2e"),
        hair("bob", "#c48a4a"), body("#3d4a5c", "fair"), hivis(),
        face("fair", "open", "stern", "flat", extra=glasses()), hardhat("#f2f2f2"), pencil())),
    "Bea": ("Banjo Bea", lambda: svg("Banjo Bea",
        background("#43e6d0", "#13262a"),
        hair("braids", "#d9733a"), body("#b83a4b", "light", collar="#f2e6d0"),
        '<path d="M60 160l80 0M70 150v54M130 150v54" stroke="#7a1f2c" stroke-width="3" opacity=".6"/>',
        face("light", "open", "kind", "grin", extra=freckles() + blush()), cowboy("#c9a36b"), banjo())),
    "Diaz": ("Flagger Diaz", lambda: svg("Flagger Diaz",
        background("#ff8a2a", "#2a130a"),
        body("#2f3e2f", "tan"), hivis("#d8f03a"),
        face("tan", "open", "stern", "flat", extra=mustache("#2a1a12")), hardhat("#ff8a2a"), stop_paddle())),
    "Rhee": ("Lot Boss Rhee", lambda: svg("Lot Boss Rhee",
        background("#ff4fa3", "#2a0d1d"),
        hair("short", "#141018"), body("#1d1a24", "light"), hivis(),
        face("light", "squint", "stern", "frown"), hardhat("#f2c14e", "BOSS"))),
    "MaOre": ("Ma Ore", lambda: svg("Ma Ore",
        background("#d8b45a", "#2b1d0c"),
        lamp_glow(), hair("bun", "#c9c3cf"), body("#6a4a8c", "medium", collar="#f2e6d0"),
        '<path d="M70 150q30 10 60 0v60H70z" fill="#efe1c4"/>',
        face("medium", "closed", "kind", "smile", extra=blush() + glasses("#8a6a1f")), mining_helmet())),
    "Dusty": ("Sheriff Dusty", lambda: svg("Sheriff Dusty",
        background("#ff8a5e", "#2a1210"),
        body("#c9b18a", "tan"), '<path d="M30 204c4-30 18-48 44-56l10 56zM170 204c-4-30-18-48-44-56l-10 56z" fill="#5a3a24"/>',
        face("tan", "open", "raised", "smile", extra=mustache("#d9d4cc", big=True)), cowboy("#e8dcc4", "#5a3a24"), star_badge())),
    "Brad": ("Brad from LIQUIDATOR", lambda: svg("Brad from LIQUIDATOR",
        background("#8cff3a", "#1a0d2e"),
        sparkles("#8cff3a"), hair("spiky", "#f5e27a"), body("#ff4fa3", "fair"),
        '<path d="M74 146q26 18 52 0l6 60H68z" fill="#fff" opacity=".18"/>',
        face("fair", "wide", "raised", "scream"),
        '<path d="M74 62h52" stroke="#111" stroke-width="7" stroke-linecap="round"/><rect x="74" y="56" width="22" height="10" rx="4" fill="#111"/><rect x="104" y="56" width="22" height="10" rx="4" fill="#111"/>',
        energy_can())),
    "NotGary": ("Not Gary", lambda: svg("Not Gary",
        background("#3a4a8c", "#07080f"),
        wind(), body("#2b3550", "light", neck=False), hood("#3b4566"),
        eyes("glow"), '<path d="M96 116q4 3 8 0" stroke="#43e6d0" stroke-opacity=".6" stroke-width="2" fill="none" stroke-linecap="round"/>',
        phone(66, 112))),
    "Gus": ("Old Gus", lambda: svg("Old Gus",
        background("#c9822e", "#22140a"),
        body("#4a5a7a", "light"),
        '<path d="M74 146l-4 60M126 146l4 60" stroke="#a83a2c" stroke-width="6"/>',
        face("light", "squint", "raised", "gap", bc="#e8e4dc", extra=beard("#e8e4dc") + mouth("gap")), prospector_hat(), pickaxe())),
    "VantageCaller": ("Vantage Wellness (on the phone)", lambda: svg("Vantage Wellness",
        background("#bfe8ff", "#4a5ea8"),
        body("#f4f7fb", "fair", collar="#8fd3c8"),
        '<circle cx="100" cy="92" r="40" fill="#e9f6ff"/><circle cx="100" cy="92" r="40" fill="none" stroke="#8fd3c8" stroke-width="3"/>',
        eyes("closed"), '<path d="M84 110q16 14 32 0" stroke="#1a1424" stroke-width="3" fill="none" stroke-linecap="round"/>',
        '<ellipse cx="80" cy="104" rx="7" ry="3.5" fill="#ffb3cf" opacity=".6"/><ellipse cx="120" cy="104" rx="7" ry="3.5" fill="#ffb3cf" opacity=".6"/>',
        headset("#c8d6e6", "#8fd3c8"),
        '<circle cx="128" cy="170" r="9" fill="#8fd3c8"/><text x="128" y="174" text-anchor="middle" font-family="Verdana,sans-serif" font-size="10" font-weight="bold" fill="#fff">V</text>',
        sparkles("#ffffff"))),
    "Holt": ("Holt (car owner)", lambda: svg("Holt",
        background("#ff6b6b", "#2a0d10"),
        body("#3f5a7a", "deep", collar="#2b4058"),
        face("deep", "wide", "angry", "shout"), ballcap("#c23a3a", "#8a2222"), receipt())),
    "Nia": ("Nia (car owner)", lambda: svg("Nia",
        background("#ff4fa3", "#250a1c"),
        hair("ponytail", "#24160f"), body("#6b7f99", "medium"),
        '<path d="M84 146q16 14 32 0" stroke="#fff" stroke-opacity=".5" stroke-width="3" fill="none"/>',
        face("medium", "open", "angry", "shout", extra=earrings()))),
    "Pryce": ("Pryce (car owner)", lambda: svg("Pryce",
        background("#ffb347", "#2a160a"),
        body("#2e6b5e", "fair", collar="#245549"),
        face("fair", "wide", "angry", "shout", extra=freckles("#c98a6a")), beanie("#5a5a6a", "#ffb347"), name_tag("PRYCE"))),
}


if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    for sid, (name, draw) in CAST.items():
        (OUT / f"{sid}.svg").write_text(draw(), encoding="utf-8")
    print(f"{len(CAST)} portraits in {OUT}")
