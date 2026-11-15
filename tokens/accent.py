#!/usr/bin/env python3
#
# The accent colour a user picks in Settings, written as a whole colour
# scheme. Plasma's own accent code blends the accent into the scheme's
# selection and links (70 % over the view background), which turns
# Raye's violet into a dull one and leaves the selection's link colour
# unreadable. Here every accent shade comes from the token rule instead
# (generate.derive_accent), the same rule the Qt style and Tokens.qml
# run, so a picked accent looks as finished as the default one.
#
#   raye-accent apply '#2672d9'   the user's scheme for that accent
#   raye-accent repair            once per account (kconf_update): drop
#                                 the AccentColor an older Raye left and
#                                 write the scheme it stood for, or write
#                                 a picked accent again with today's greys
#
# The default accent gives back RayeNight or RayeDay, byte for byte the
# shipped scheme. Another accent gives RayeNightAccent or RayeDayAccent,
# written to the user's color-schemes folder so Plasma's check at login
# (ColorSchemeHash) finds the file and keeps it.

import hashlib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))
import generate as g  # noqa: E402

CUSTOM_SUFFIX = "Accent"


# --- the scheme -------------------------------------------------------

def normalise(accent):
    """'#rrggbb' (Qt) or 'r,g,b' (KConfig) -> '#RRGGBB', None for anything else."""
    accent = accent.strip()
    m = re.fullmatch(r"#?([0-9A-Fa-f]{6})", accent)
    if m:
        return "#" + m.group(1).upper()
    m = re.fullmatch(r"(\d{1,3}),(\d{1,3}),(\d{1,3})(,\d{1,3})?", accent.replace(" ", ""))
    if m and all(int(v) <= 255 for v in m.groups()[:3]):
        return g.hexstr(tuple(int(v) for v in m.groups()[:3]))
    return None


def mode_of(scheme_name, tokens):
    """'night' or 'day' for one of Raye's schemes, None for anything else."""
    for mode, info in tokens["modes"].items():
        if scheme_name in (info["scheme"], info["scheme"] + CUSTOM_SUFFIX):
            return mode
    return None


def is_default(tokens, accent):
    return accent.upper() == tokens["accent_rule"]["default"].upper()


def accent_tokens(tokens, accent):
    """The token table with every accent shade taken from the rule.

    The five tinted foregrounds of a selected row go with them. They were
    constants tuned by eye for the violet -- three of them with no rule at
    all -- and a constant on a moving background is a ratio nobody holds:
    on an amber accent selection.negative sat at 1.92:1.
    """
    out = dict(tokens)
    colours = dict(tokens["color"])
    derived = {mode: g.derive_accent(tokens, accent, mode) for mode in g.MODES}
    for name in g.ACCENT_TOKENS:
        colours[name] = dict(colours[name], **{m: derived[m][name] for m in g.MODES})
    tints = {m: g.derive_selection(tokens, accent, m, derived[m]) for m in g.MODES}
    for name in g.SELECTION_TINTS:
        colours[name] = dict(colours[name], **{m: tints[m][name] for m in g.MODES})
    out["color"] = colours
    # white links on the selection hold on violet, not on a light accent
    kde = dict(tokens["kde"])
    groups = dict(kde["groups"])
    groups["Selection"] = dict(groups["Selection"], ForegroundActive="text.onAccent",
                               ForegroundLink="text.onAccent")
    out["kde"] = dict(kde, groups=groups)
    return out


def scheme(tokens, accent, mode):
    """(scheme name, text of the .colors file) for an accent and a mode."""
    info = tokens["modes"][mode]
    if is_default(tokens, accent):
        return info["scheme"], g.colors_file(tokens, mode)
    name = info["scheme"] + CUSTOM_SUFFIX
    modes = dict(tokens["modes"])
    modes[mode] = dict(info, scheme=name)
    text = g.colors_file(dict(accent_tokens(tokens, accent), modes=modes), mode)
    return name, text


# --- kdeglobals -------------------------------------------------------

def parse(text):
    """KDE ini text -> [(header or None, [lines])], comments and all."""
    groups = [(None, [])]
    for line in text.splitlines():
        stripped = line.strip()
        # "[$i]" alone locks the whole file; "[$Version]" is a group
        if stripped.startswith("[") and stripped.endswith("]") and not re.fullmatch(r"\[\$[a-z]\]", stripped):
            groups.append((stripped, []))
        else:
            groups[-1][1].append(line)
    return groups


def unparse(groups):
    out = []
    for header, lines in groups:
        while lines and not lines[-1].strip():
            lines = lines[:-1]
        if header is None and not lines:
            continue
        if out:
            out.append("")
        if header is not None:
            out.append(header)
        out.extend(lines)
    return "\n".join(out) + "\n"


def key_of(line):
    if "=" not in line or line.lstrip().startswith("#"):
        return None
    key = line.split("=", 1)[0].strip()
    return re.sub(r"\[\$[a-z]+\]$", "", key)


def entries(groups, header):
    for h, lines in groups:
        if h == header:
            return {key_of(l): l.split("=", 1)[1] for l in lines if key_of(l)}
    return {}


def set_entries(groups, header, values, remove=()):
    """A copy of groups with keys set (in place where they were, at the
    end of the group otherwise) and others taken out."""
    out, done = [], False
    for h, lines in groups:
        if h == header and not done:
            done = True
            left = dict(values)
            new = []
            for line in lines:
                key = key_of(line)
                if key in remove:
                    continue
                if key in left:
                    new.append("%s=%s" % (key, left.pop(key)))
                else:
                    new.append(line)
            while new and not new[-1].strip():
                new.pop()
            new += ["%s=%s" % kv for kv in left.items()]
            out.append((h, new))
        else:
            out.append((h, lines))
    if not done and values:
        out.append((header, ["%s=%s" % kv for kv in values.items()]))
    return out


# what applying a scheme copies into kdeglobals, as Plasma does
COPIED_GROUPS = re.compile(r"^\[(Colors:|ColorEffects:|WM\])")


def apply(globals_text, name, scheme_text):
    """kdeglobals with a scheme's colours in it and no AccentColor."""
    groups = parse(globals_text)
    for header, _ in parse(scheme_text):
        if header and COPIED_GROUPS.match(header):
            groups = set_entries(groups, header, entries(parse(scheme_text), header))
    digest = hashlib.sha1(scheme_text.encode("utf-8")).hexdigest()
    return unparse(set_entries(groups, "[General]",
                               {"ColorScheme": name, "ColorSchemeHash": digest},
                               remove=("AccentColor",)))


def repair_accent(globals_text, tokens, mode):
    """The accent an account's kdeglobals should be rewritten with, None
    when there is nothing to repair.

    An AccentColor in the account's file is the accent Plasma blended in.
    Without one, Plasma may still have blended the system file's
    AccentColor into the account's colours: a Raye Night or Day account
    whose colours are not the scheme's gets the scheme back, and a picked
    accent whose colours are not today's is written again. An accent
    that follows the wallpaper is Plasma's to keep up to date."""
    general = entries(parse(globals_text), "[General]")
    if mode is None or general.get("accentColorFromWallpaper", "").strip().lower() == "true":
        return None
    if "AccentColor" in general:
        return normalise(general["AccentColor"])
    groups = parse(globals_text)
    name = general.get("ColorScheme", "").strip()
    if name == tokens["modes"][mode]["scheme"]:
        accent = tokens["accent_rule"]["default"].upper()
    elif name == tokens["modes"][mode]["scheme"] + CUSTOM_SUFFIX:
        # a picked accent written with the greys of an older Raye; the
        # selection is the accent itself
        accent = normalise(entries(groups, "[Colors:Selection]").get("BackgroundNormal", ""))
        if accent is None:
            return None
    else:
        return None
    want = parse(scheme(tokens, accent, mode)[1])
    for header, _ in want:
        if header and header.startswith("[Colors:"):
            mine = entries(groups, header)
            if any(mine.get(k, v) != v for k, v in entries(want, header).items()):
                return accent
    return None


# --- files and the session --------------------------------------------

def config_home():
    return os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")


def data_home():
    return os.environ.get("XDG_DATA_HOME") or os.path.expanduser("~/.local/share")


def config_dirs():
    return (os.environ.get("XDG_CONFIG_DIRS") or "/etc/xdg").split(":")


def read(path):
    try:
        with open(path, encoding="utf-8") as f:
            return f.read()
    except FileNotFoundError:
        return ""


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".raye-accent"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    if os.path.exists(path):
        os.chmod(tmp, os.stat(path).st_mode & 0o7777)
    os.replace(tmp, path)


def current_scheme(user_text):
    """ColorScheme as KDE reads it: the user's file over the system ones."""
    for text in [user_text] + [read(os.path.join(d, "kdeglobals")) for d in config_dirs()]:
        name = entries(parse(text), "[General]").get("ColorScheme")
        if name:
            return name.strip()
    return ""


def notify(groups):
    """Tell running programs, as Plasma does after applying a scheme:
    KConfigWatcher users (GTK settings, the shell) and the palette."""
    # byte arrays spelled out: b'...' would carry a trailing NUL
    def ay(key):
        return "[byte " + ", ".join("0x%02x" % c for c in key.encode("utf-8")) + "]"
    changed = ", ".join("'%s': [%s]" % (group, ", ".join(ay(k) for k in keys))
                        for group, keys in sorted(groups.items()))
    for argv in (["gdbus", "emit", "--session", "--object-path", "/kdeglobals",
                  "--signal", "org.kde.kconfig.notify.ConfigChanged", "{%s}" % changed],
                 ["gdbus", "emit", "--session", "--object-path", "/KGlobalSettings",
                  "--signal", "org.kde.KGlobalSettings.notifyChange", "0", "0"]):
        try:
            subprocess.run(argv, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5)
        except (OSError, subprocess.TimeoutExpired):
            return


def write_accent(tokens, accent, mode):
    path = os.path.join(config_home(), "kdeglobals")
    name, text = scheme(tokens, accent, mode)
    own = os.path.join(data_home(), "color-schemes",
                       tokens["modes"][mode]["scheme"] + CUSTOM_SUFFIX + ".colors")
    if name == tokens["modes"][mode]["scheme"]:
        if os.path.exists(own):
            os.remove(own)
    else:
        write(own, text)
    write(path, apply(read(path), name, text))
    changed = {"General": ["ColorScheme", "AccentColor"]}
    for header, _ in parse(text):
        if header and COPIED_GROUPS.match(header):
            changed[header[1:-1].replace("][", "\x1d")] = sorted(entries(parse(text), header))
    notify({k: v for k, v in changed.items() if "\x1d" not in k})


def main(argv):
    tokens = g.load()
    if argv[:1] == ["apply"] and len(argv) == 2:
        accent = normalise(argv[1])
        if accent is None:
            print("not a colour: %s" % argv[1], file=sys.stderr)
            return 2
        user = read(os.path.join(config_home(), "kdeglobals"))
        mode = mode_of(current_scheme(user), tokens)
        if mode is None:
            # not a Raye scheme: Plasma's own accent code is all there is
            try:
                return subprocess.call(["plasma-apply-colorscheme", "--accent-color", accent])
            except OSError:
                return 1
        write_accent(tokens, accent, mode)
        return 0
    if argv == ["repair"]:
        user = read(os.path.join(config_home(), "kdeglobals"))
        mode = mode_of(current_scheme(user), tokens)
        accent = repair_accent(user, tokens, mode)
        if accent:
            write_accent(tokens, accent, mode)
        return 0
    print("usage: raye-accent apply COLOUR | repair", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
