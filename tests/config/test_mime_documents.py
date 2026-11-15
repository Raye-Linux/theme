# config/mime/raye-documents.xml: everyday file types the stock list does
# not know get a name and an icon the Raye themes draw.

import os
import xml.etree.ElementTree as ET

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
XML = os.path.join(ROOT, "config", "mime", "raye-documents.xml")
NS = {"m": "http://www.freedesktop.org/standards/shared-mime-info"}
LANG = "{http://www.w3.org/XML/1998/namespace}lang"
STOCK = "/usr/share/mime/packages/freedesktop.org.xml"
# seen as a blank "Unknown" page on an installed system
WANTED = {"one", "onetoc2", "accdb", "msg", "cmd", "webloc", "maff"}


def types():
    return [t for t in ET.parse(XML).getroot().findall("m:mime-type", NS)
            if t.get("type") != "application/octet-stream"]


def globs(element):
    return {g.get("pattern") for g in element.findall("m:glob", NS)}


def test_the_types_are_all_there():
    found = set()
    for t in types():
        found |= {g.removeprefix("*.") for g in globs(t)}
    assert WANTED <= found


@pytest.mark.parametrize("mime", types(), ids=lambda t: t.get("type"))
def test_each_type_says_what_it_is_in_english_and_french(mime):
    comments = mime.findall("m:comment", NS)
    assert {c.get(LANG) for c in comments} == {None, "fr"}
    assert all(c.text and c.text.strip() for c in comments)


@pytest.mark.parametrize("mime", types(), ids=lambda t: t.get("type"))
def test_each_icon_is_one_raye_draws(mime):
    icon = mime.find("m:generic-icon", NS)
    assert icon is not None
    path = os.path.join(ROOT, "icons", "Raye", "mimetypes", "48", icon.get("name") + ".svg")
    assert os.path.exists(path), icon.get("name")


def test_files_of_no_known_type_have_a_french_name():
    root = ET.parse(XML).getroot()
    mime = root.find("m:mime-type[@type='application/octet-stream']", NS)
    assert mime.find("m:comment", NS).get(LANG) == "fr"


def test_the_package_installs_it():
    with open(os.path.join(ROOT, "debian", "raye-theme.install")) as f:
        assert "config/mime/raye-documents.xml usr/share/mime/packages" in f.read()


@pytest.mark.skipif(not os.path.exists(STOCK), reason="needs shared-mime-info")
def test_no_other_type_already_claims_these_files():
    stock = ET.parse(STOCK).getroot().findall("m:mime-type", NS)
    ours = {g: t.get("type") for t in types() for g in globs(t)}
    for t in stock:
        for g in globs(t) & set(ours):
            assert t.get("type") == ours[g], g
