"""Static contract checks for the approved WebView v3 UI; no third-party packages."""
from html.parser import HTMLParser
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HTML = (ROOT / "plugin/ui/web/ProphecY_v3_0.html").read_text(encoding="utf-8")
CPP = (ROOT / "plugin/ui/web/ProphecYWebEditor.cpp").read_text(encoding="utf-8")


class Scan(HTMLParser):
    def __init__(self):
        super().__init__()
        self.scripts = []
        self.current = None
        self.knobs = 0
        self.buttons = 0

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        classes = attrs.get("class", "").split()
        if "knob-slot" in classes:
            self.knobs += 1
        if "tactile-btn" in classes:
            self.buttons += 1
        if tag == "script":
            self.current = []

    def handle_data(self, data):
        if self.current is not None:
            self.current.append(data)

    def handle_endtag(self, tag):
        if tag == "script" and self.current is not None:
            self.scripts.append("".join(self.current))
            self.current = None


scan = Scan()
scan.feed(HTML)
assert scan.knobs == 39, f"Expected 39 knobs, got {scan.knobs}"
assert scan.buttons == 21, f"Expected 21 tactile switches, got {scan.buttons}"
assert len(scan.scripts) == 3, f"Expected three inline scripts, got {len(scan.scripts)}"

for index, script in enumerate(scan.scripts):
    with tempfile.TemporaryDirectory() as tmp:
        js = Path(tmp) / f"webview_script_{index}.js"
        js.write_text(script, encoding="utf-8")
        subprocess.run(["node", "--check", str(js)], check=True)

bridge = scan.scripts[-1]
def ids_of(name):
    match = re.search(r"const " + re.escape(name) + r"=\[([^;]+)\];", bridge)
    assert match, f"Missing JS mapping {name}"
    return re.findall(r"['\\"]([^'\\"]+)['\\"]", match.group(1))

knob_ids = ids_of("ids")
fader_ids = ids_of("faderIds")
switch_ids = ids_of("switchIds")
def cpp_ids(name):
    match = re.search(r"k" + name + r"Nrpn\{([^}]+)\}", CPP)
    assert match, f"Missing C++ mapping {name}"
    return ["nrpn_" + str(int(x)).zfill(4) for x in re.findall(r"\d+", match.group(1))]

assert knob_ids == cpp_ids("Knob"), "JS and C++ knob mapping diverged"
assert fader_ids == cpp_ids("Fader"), "JS and C++ fader mapping diverged"
assert switch_ids[:14] == cpp_ids("Switch"), "JS and C++ switch mapping diverged"
assert len(switch_ids) == 17, "Arp switches not connected"
assert "character_tube_drive" in bridge and "character_spring_mix" in bridge
assert "character_bypass" in CPP and "character_custom_type" in CPP
assert "window.__prophecyUpdateADSR?.()" in bridge
assert "rotaryButton.disabled=true" in bridge, "Unsupported rotary should not be interactive"
assert "prophecyReady" in bridge and "prophecyHostState" in bridge
print("WebView v3 HTML syntax, control counts and C++/JS parameter contracts OK")
