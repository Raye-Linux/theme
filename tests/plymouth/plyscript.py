# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs raye.script the way plymouth's script plugin does, on fake screens.

plymouthd needs root and its own mounts to draw, so the package build
cannot start it. This reads the same script, with the subset of the
language and of the Image/Sprite/Window/Plymouth objects the theme uses,
and keeps every sprite with its picture so a test can see where things
land on each screen. tests/plymouth/run.sh does the same with the real
plymouthd under Xvfb.

Screens are laid out like plymouth does it: one canvas as big as the
biggest screen, each screen centred in it."""

import math
import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_DIR = Path("/usr/share/fonts/opentype/inter")
FONT_FILES = {"": "Inter-Regular.otf", "semi-bold": "Inter-SemiBold.otf", "bold": "Inter-Bold.otf"}


class ScriptError(Exception):
    pass


# ---- tokens and syntax ----
TOKEN = re.compile(r"""
    (?P<ws>\s+|\#[^\n]*|//[^\n]*|/\*.*?\*/)
  | (?P<num>\d+\.\d*|\.\d+|\d+)
  | (?P<str>"(?:\\.|[^"\\])*")
  | (?P<name>[A-Za-z_]\w*)
  | (?P<op>==|!=|<=|>=|&&|\|\||\+\+|--|\+=|-=|\*=|/=|[-+*/%<>=!(){}\[\];,.])
""", re.S | re.X)


def tokenize(src):
    out, pos = [], 0
    while pos < len(src):
        m = TOKEN.match(src, pos)
        if not m:
            raise ScriptError(f"bad character at {pos}: {src[pos:pos + 20]!r}")
        pos = m.end()
        kind = m.lastgroup
        if kind == "ws":
            continue
        text = m.group()
        if kind == "num":
            out.append(("num", float(text)))
        elif kind == "str":
            body = text[1:-1]
            body = re.sub(r"\\(.)", lambda e: {"n": "\n", "t": "\t"}.get(e.group(1), e.group(1)), body)
            out.append(("str", body))
        else:
            out.append((kind, text))
    out.append(("end", None))
    return out


class Parser:
    def __init__(self, src):
        self.toks = tokenize(src)
        self.i = 0

    def peek(self, k=0):
        return self.toks[self.i + k]

    def take(self, want=None):
        t = self.toks[self.i]
        if want is not None and t[1] != want:
            raise ScriptError(f"expected {want!r}, got {t[1]!r} (token {self.i})")
        self.i += 1
        return t

    def at(self, text):
        return self.toks[self.i][1] == text and self.toks[self.i][0] in ("op", "name")

    def program(self):
        body = []
        while self.peek()[0] != "end":
            body.append(self.statement())
        return ("block", body)

    def statement(self):
        if self.at("{"):
            self.take("{")
            body = []
            while not self.at("}"):
                body.append(self.statement())
            self.take("}")
            return ("block", body)
        if self.at(";"):
            self.take()
            return ("block", [])
        if self.at("fun"):
            self.take()
            name = self.take()[1]
            self.take("(")
            params = []
            while not self.at(")"):
                params.append(self.take()[1])
                if self.at(","):
                    self.take()
            self.take(")")
            return ("fun", name, params, self.statement())
        if self.at("if"):
            self.take()
            self.take("(")
            cond = self.expr()
            self.take(")")
            then = self.statement()
            other = None
            if self.at("else"):
                self.take()
                other = self.statement()
            return ("if", cond, then, other)
        if self.at("while"):
            self.take()
            self.take("(")
            cond = self.expr()
            self.take(")")
            return ("while", cond, self.statement())
        if self.at("for"):
            self.take()
            self.take("(")
            init = None if self.at(";") else self.expr()
            self.take(";")
            cond = None if self.at(";") else self.expr()
            self.take(";")
            step = None if self.at(")") else self.expr()
            self.take(")")
            return ("for", init, cond, step, self.statement())
        if self.at("return"):
            self.take()
            value = None if self.at(";") else self.expr()
            self.take(";")
            return ("return", value)
        e = self.expr()
        self.take(";")
        return ("expr", e)

    def expr(self):
        left = self.binary(0)
        if self.at("=") or self.at("+=") or self.at("-=") or self.at("*=") or self.at("/="):
            op = self.take()[1]
            return ("assign", op, left, self.expr())
        return left

    LEVELS = [("||",), ("&&",), ("==", "!="), ("<", "<=", ">", ">="), ("+", "-"), ("*", "/", "%")]

    def binary(self, level):
        if level == len(self.LEVELS):
            return self.unary()
        left = self.binary(level + 1)
        while self.peek()[0] == "op" and self.peek()[1] in self.LEVELS[level]:
            op = self.take()[1]
            left = ("bin", op, left, self.binary(level + 1))
        return left

    def unary(self):
        if self.peek()[0] == "op" and self.peek()[1] in ("!", "-", "+"):
            op = self.take()[1]
            return ("un", op, self.unary())
        return self.postfix()

    def postfix(self):
        e = self.primary()
        while True:
            if self.at("."):
                self.take()
                e = ("field", e, self.take()[1])
            elif self.at("["):
                self.take()
                k = self.expr()
                self.take("]")
                e = ("index", e, k)
            elif self.at("("):
                self.take()
                args = []
                while not self.at(")"):
                    args.append(self.expr())
                    if self.at(","):
                        self.take()
                self.take(")")
                e = ("call", e, args)
            elif self.at("++") or self.at("--"):
                e = ("post", self.take()[1], e)
            else:
                return e

    def primary(self):
        kind, text = self.take()
        if kind in ("num", "str"):
            return ("const", text)
        if kind == "name":
            if text == "NULL":
                return ("const", None)
            return ("name", text)
        if text == "(":
            e = self.expr()
            self.take(")")
            return e
        raise ScriptError(f"unexpected {text!r}")


# ---- values ----
class Hash(dict):
    pass


class Function:
    def __init__(self, name, params, body):
        self.name, self.params, self.body = name, params, body


class Native:
    def __init__(self, fn):
        self.fn = fn


class Return(Exception):
    def __init__(self, value):
        self.value = value


def font_of(spec):
    """'Inter Semi-Bold 20px' -> a Pillow font."""
    m = re.match(r"(.*?)\s+(\d+)px$", spec)
    words, size = m.group(1).split(), int(m.group(2))
    weight = " ".join(words[1:]).lower()
    return ImageFont.truetype(str(FONT_DIR / FONT_FILES[weight]), size)


class Img:
    def __init__(self, pil):
        self.pil = pil
        self._ink = False

    @property
    def ink(self):
        """Box of what shows, the faint track of the ring included."""
        if self._ink is False:
            self._ink = self.pil.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
        return self._ink

    def method(self, name, args):
        if name == "GetWidth":
            return float(self.pil.width)
        if name == "GetHeight":
            return float(self.pil.height)
        if name == "Scale":
            w, h = max(1, int(args[0])), max(1, int(args[1]))
            return Img(self.pil.resize((w, h), Image.BILINEAR))
        if name == "Crop":
            x, y, w, h = (int(a) for a in args)
            return Img(self.pil.crop((x, y, x + w, y + h)))
        raise ScriptError(f"Image.{name}")


def text_image(text, r, g, b, a, font):
    """What label-pango hands back: the line's logical box, glyphs on it."""
    f = font_of(font)
    ascent, descent = f.getmetrics()
    w = max(1, math.ceil(f.getlength(text)))
    img = Image.new("RGBA", (w, ascent + descent), (0, 0, 0, 0))
    ImageDraw.Draw(img).text((0, 0), text, font=f,
                             fill=(int(r * 255), int(g * 255), int(b * 255), int(a * 255)))
    return Img(img)


class Sprite:
    def __init__(self, img):
        self.img = img
        self.x = self.y = self.z = 0.0
        self.opacity = 1.0

    def method(self, name, args):
        if name == "SetPosition":
            self.x, self.y = args[0], args[1]
            if len(args) > 2:
                self.z = args[2]
        elif name == "SetX":
            self.x = args[0]
        elif name == "SetY":
            self.y = args[0]
        elif name == "SetZ":
            self.z = args[0]
        elif name == "SetOpacity":
            self.opacity = args[0]
        elif name == "SetImage":
            self.img = args[0]
        elif name == "GetImage":
            return self.img
        else:
            raise ScriptError(f"Sprite.{name}")
        return None


def string_method(s, name, args):
    if name == "Length":
        return float(len(s))
    if name == "SubString":
        a, b = int(args[0]), int(args[1])
        return s[a:b]
    if name == "CharAt":
        i = int(args[0])
        return s[i] if 0 <= i < len(s) else ""
    raise ScriptError(f"String.{name}")


def to_text(v):
    if v is None:
        return "NULL"
    if isinstance(v, float):
        return str(int(v)) if v == int(v) else repr(v)
    return str(v)


def truthy(v):
    if v is None:
        return False
    if isinstance(v, float):
        return v != 0
    if isinstance(v, str):
        return True
    return True


class Plymouth:
    """One plymouthd with screens of the given sizes, running a script."""

    def __init__(self, script, theme_dir, screens, mode="boot", files=None):
        self.theme_dir = Path(theme_dir)
        self.files = files or {}
        self.mode = mode
        self.callbacks = {}
        self.sprites = []
        self.set_screens(screens)
        self.globals = Hash()
        self.globals.update(self.natives())
        self.run(Parser(script).program(), self.globals)

    # ---- screens ----
    def set_screens(self, sizes):
        mw = max(w for w, _ in sizes)
        mh = max(h for _, h in sizes)
        self.canvas = (mw, mh)
        self.screens = [((mw - w) // 2, (mh - h) // 2, w, h) for w, h in sizes]

    def window(self, what):
        def get(i=None):
            if i is None:
                return float(self.canvas[0] if what == "w" else self.canvas[1] if what == "h" else 0)
            i = int(i)
            if i < 0 or i >= len(self.screens):
                return None
            x, y, w, h = self.screens[i]
            return float({"x": x, "y": y, "w": w, "h": h}[what])
        return get

    # ---- the objects a script sees ----
    def natives(self):
        me = self

        def load(name):
            name = self.files.get(name, name)
            p = self.theme_dir / name
            if not p.exists():
                return None
            return Img(Image.open(p).convert("RGBA"))

        image = Hash(Text=Native(lambda *a: text_image(*a)))
        image["__call__"] = Native(load)

        def new_sprite(img=None):
            s = Sprite(img)
            me.sprites.append(s)
            return s

        sprite = Hash(__call__=Native(new_sprite))
        window = Hash(
            GetWidth=Native(self.window("w")), GetHeight=Native(self.window("h")),
            GetX=Native(self.window("x")), GetY=Native(self.window("y")),
            SetBackgroundTopColor=Native(lambda *a: None),
            SetBackgroundBottomColor=Native(lambda *a: None))
        mth = Hash(
            Int=Native(lambda v: float(int(v))), Abs=Native(lambda v: abs(v)),
            Min=Native(lambda a, b: min(a, b)), Max=Native(lambda a, b: max(a, b)),
            Clamp=Native(lambda v, a, b: min(max(v, a), b)), Sin=Native(math.sin),
            Cos=Native(math.cos), Sqrt=Native(math.sqrt), Pi=math.pi)

        def setter(key):
            return Native(lambda fn: me.callbacks.__setitem__(key, fn))

        ply = Hash(
            SetRefreshRate=Native(lambda r: None),
            GetMode=Native(lambda: me.mode),
            GetCapslockState=Native(lambda: 0.0))
        for key in ("Refresh", "DisplayMessage", "HideMessage", "UpdateStatus", "SystemUpdate",
                    "DisplayPassword", "DisplayQuestion", "DisplayNormal", "Quit",
                    "BootProgress", "RootMounted", "KeyboardInput", "MessageFunction"):
            ply["Set" + key + "Function"] = setter(key)
        return Hash(Image=image, Sprite=sprite, Window=window, Math=mth, Plymouth=ply,
                    String=Native(lambda v: to_text(v) if not isinstance(v, str) else v))

    # ---- running ----
    def call(self, fn, args):
        if isinstance(fn, Native):
            return fn.fn(*args)
        if isinstance(fn, Hash) and "__call__" in fn:
            return fn["__call__"].fn(*args)
        if not isinstance(fn, Function):
            raise ScriptError(f"not a function: {fn!r}")
        local = Hash()
        for i, p in enumerate(fn.params):
            local[p] = args[i] if i < len(args) else None
        try:
            self.run(fn.body, local)
        except Return as r:
            return r.value
        return None

    def callback(self, key, *args):
        fn = self.callbacks.get(key)
        if fn is not None:
            return self.call(fn, list(args))
        return None

    def run(self, node, local):
        kind = node[0]
        if kind == "block":
            for s in node[1]:
                self.run(s, local)
        elif kind == "expr":
            self.eval(node[1], local)
        elif kind == "fun":
            local[node[1]] = Function(node[1], node[2], node[3])
        elif kind == "if":
            if truthy(self.eval(node[1], local)):
                self.run(node[2], local)
            elif node[3] is not None:
                self.run(node[3], local)
        elif kind == "while":
            n = 0
            while truthy(self.eval(node[1], local)):
                self.run(node[2], local)
                n += 1
                if n > 100000:
                    raise ScriptError("loop does not end")
        elif kind == "for":
            if node[1] is not None:
                self.eval(node[1], local)
            while node[2] is None or truthy(self.eval(node[2], local)):
                self.run(node[4], local)
                if node[3] is not None:
                    self.eval(node[3], local)
        elif kind == "return":
            raise Return(None if node[1] is None else self.eval(node[1], local))
        else:
            raise ScriptError(f"statement {kind}")

    # a place a value can be written to: (container, key)
    def place(self, node, local, make=True):
        kind = node[0]
        if kind == "name":
            name = node[1]
            if name in local:
                return local, name
            if name in self.globals:
                return self.globals, name
            return local, name
        if kind in ("field", "index"):
            key = node[2] if kind == "field" else self.eval(node[2], local)
            if isinstance(key, float):
                key = int(key)
            if node[1] == ("name", "global"):
                return self.globals, key
            if node[1] == ("name", "local"):
                return local, key
            box, k = self.place(node[1], local)
            obj = box.get(k)
            if not isinstance(obj, Hash):
                obj = Hash()
                box[k] = obj
            return obj, key
        raise ScriptError(f"cannot assign to {kind}")

    def eval(self, node, local):
        kind = node[0]
        if kind == "const":
            return node[1]
        if kind == "name":
            name = node[1]
            if name == "global":
                return self.globals
            if name == "local":
                return local
            if name in local:
                return local[name]
            return self.globals.get(name)
        if kind in ("field", "index"):
            obj = self.eval(node[1], local)
            key = node[2] if kind == "field" else self.eval(node[2], local)
            if isinstance(key, float):
                key = int(key)
            if isinstance(obj, Hash):
                return obj.get(key)
            return None
        if kind == "call":
            args = [self.eval(a, local) for a in node[2]]
            callee = node[1]
            if callee[0] == "field":
                obj = self.eval(callee[1], local)
                if isinstance(obj, (Img, Sprite)):
                    return obj.method(callee[2], args)
                if isinstance(obj, str):
                    return string_method(obj, callee[2], args)
            return self.call(self.eval(callee, local), args)
        if kind == "assign":
            op, target, value = node[1], node[2], self.eval(node[3], local)
            box, key = self.place(target, local)
            if op != "=":
                value = self.binop(op[0], box.get(key), value)
            box[key] = value
            return value
        if kind == "post":
            box, key = self.place(node[2], local)
            old = box.get(key) or 0.0
            box[key] = old + (1 if node[1] == "++" else -1)
            return old
        if kind == "un":
            v = self.eval(node[2], local)
            if node[1] == "!":
                return 0.0 if truthy(v) else 1.0
            return -v if node[1] == "-" else v
        if kind == "bin":
            op = node[1]
            if op == "&&":
                a = self.eval(node[2], local)
                return 1.0 if truthy(a) and truthy(self.eval(node[3], local)) else 0.0
            if op == "||":
                a = self.eval(node[2], local)
                return 1.0 if truthy(a) or truthy(self.eval(node[3], local)) else 0.0
            return self.binop(op, self.eval(node[2], local), self.eval(node[3], local))
        raise ScriptError(f"expression {kind}")

    @staticmethod
    def binop(op, a, b):
        if op == "==":
            return 1.0 if a == b and type(a) is type(b) else 0.0
        if op == "!=":
            return 0.0 if a == b and type(a) is type(b) else 1.0
        if op == "+" and (isinstance(a, str) or isinstance(b, str)):
            return to_text(a) + to_text(b)
        if a is None or b is None:
            raise ScriptError(f"NULL {op} value")
        return float({
            "+": lambda: a + b, "-": lambda: a - b, "*": lambda: a * b, "/": lambda: a / b,
            "%": lambda: math.fmod(a, b), "<": lambda: a < b, "<=": lambda: a <= b,
            ">": lambda: a > b, ">=": lambda: a >= b}[op]())

    # ---- what a test drives ----
    def frames(self, n):
        for _ in range(n):
            self.callback("Refresh")

    def message(self, text):
        self.callback("DisplayMessage", text)

    def live_sprites(self):
        """Sprites something still holds: plymouth drops the others."""
        seen, out, todo = set(), [], [self.globals]
        while todo:
            v = todo.pop()
            if id(v) in seen:
                continue
            seen.add(id(v))
            if isinstance(v, Sprite):
                out.append(v)
            elif isinstance(v, dict):
                todo.extend(v.values())
        return out

    def ink_boxes(self, min_z=2):
        """(screen, box) for every screen: what shows on it, in its own
        pixels. Light and background (z under min_z) are left out."""
        result = []
        for sx, sy, sw, sh in self.screens:
            box = None
            for s in self.live_sprites():
                if s.z < min_z or s.opacity <= 0 or s.img is None or s.img.ink is None:
                    continue
                ix0, iy0, ix1, iy1 = s.img.ink
                x0, y0 = int(s.x) + ix0 - sx, int(s.y) + iy0 - sy
                x1, y1 = int(s.x) + ix1 - sx, int(s.y) + iy1 - sy
                if x1 <= 0 or y1 <= 0 or x0 >= sw or y0 >= sh:
                    continue
                x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, sw), min(y1, sh)
                box = (x0, y0, x1, y1) if box is None else (
                    min(box[0], x0), min(box[1], y0), max(box[2], x1), max(box[3], y1))
            result.append(((sx, sy, sw, sh), box))
        return result
