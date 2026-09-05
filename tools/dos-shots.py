#!/usr/bin/env python3
"""Run MAZE2.EXE in dosbox-x on a virtual X display, play it with held keys, and save screenshots.

  tools/dos-shots.py                 scripted tour: credits, title, walk, turn, shoot, FPS overlay, credits
  tools/dos-shots.py --photos        README screenshots (several spawns via -at) into screenshots/
  tools/dos-shots.py --cycles N      dosbox-x fixed cycle count (default 30000 ~ a fast 486/slow Pentium)
  tools/dos-shots.py --keep          leave the last dosbox-x running (for manual xdotool poking)

Tour output lands in build/dosshots/. Requires Xvfb, xdotool, ImageMagick (import).
The run directory is build/, which holds only MAZE2.EXE + FM.DAT + 1.MID: no CWSDPMI.EXE,
so a working run also proves the bound-in DPMI stub.

Input quirks learned the hard way (all handled below):
  * no window manager on Xvfb -> X keyboard focus follows the pointer, so park it in the window;
  * keys go through XTEST (real input): SDL drops some synthetic `xdotool key --window` keys;
  * dosbox-x swallows the very first key press of a session, so a throwaway key is sent at boot
    (the game flushes the BIOS buffer before the title, so it can't skip the title);
  * a ~10 ms press/release pair is often lost, so taps are held for 120 ms.
"""
import os, subprocess, sys, time, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE_DIR = os.path.join(ROOT, "build")
DISPLAY = ":97"
WIN_W, WIN_H = 640, 400          # 2x mode 13h

def sh(*a, **k): return subprocess.run(a, check=False, capture_output=True, text=True, **k)

class Session:
    """one dosbox-x process on the virtual display"""
    def __init__(self, out, cycles, args="", log="MAZE2.OUT"):
        self.out = out; os.makedirs(out, exist_ok=True)
        self.env = dict(os.environ, DISPLAY=DISPLAY)
        self.shots = []
        conf = os.path.join(out, "dosbox.conf")
        with open(conf, "w") as f:
            f.write(f"""[sdl]
autolock=false
windowresolution={WIN_W}x{WIN_H}
output=surface
[dosbox]
machine=svga_s3
memsize=16
quit warning=false
[cpu]
core=normal
cycles=fixed {cycles}
[render]
aspect=false
[sblaster]
sbtype=sb16
[speaker]
pcspeaker=true
[autoexec]
mount c {EXE_DIR}
c:
MAZE2.EXE {args} > {log}
""")
        self.xvfb = subprocess.Popen(["Xvfb", DISPLAY, "-screen", "0", "1024x768x24", "-nolisten", "tcp"],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1.0)
        self.box = subprocess.Popen(["dosbox-x", "-conf", conf, "-nomenu", "-fastlaunch"], env=self.env,
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(40):
            if self.win(): break
            time.sleep(0.5)
        else:
            self.close(); raise RuntimeError("dosbox-x window never appeared on " + DISPLAY)
        time.sleep(1.0)                # let SDL settle: a key sent earlier doesn't count as the "first"
        w = self.win()
        r = sh("xdotool", "getwindowgeometry", "--shell", w, env=self.env)
        g = dict(l.split("=") for l in r.stdout.split() if "=" in l)
        sh("xdotool", "mousemove", "--sync", str(int(g.get("X", 0)) + WIN_W // 2), str(int(g.get("Y", 0)) + WIN_H // 2), env=self.env)
        time.sleep(0.3)
        self.hold(["z"], 0.3)          # throwaway first key (see module doc)
    def win(self):
        r = sh("xdotool", "search", "--onlyvisible", "--name", "DOSBox", env=self.env)
        ids = [l for l in r.stdout.split() if l.strip()]
        return ids[-1] if ids else None
    def focus(self):
        w = self.win()
        if not w: return
        # pointer was parked in the window at start-up (focus follows it); --sync so the key that
        # follows can't race the FocusIn, which makes SDL drop it
        sh("xdotool", "windowfocus", "--sync", w, env=self.env)
    def shot(self, name, numbered=True):
        w = self.win()
        if not w: print("no window for", name); return
        fn = f"{len(self.shots):02d}_{name}.png" if numbered else f"{name}.png"
        path = os.path.join(self.out, fn)
        sh("import", "-display", DISPLAY, "-window", w, path, env=self.env)
        self.shots.append(path); print("shot", fn)
    def tap(self, k, delay=0.25):
        self.hold([k], 0.25); time.sleep(delay)
    def start_game(self):
        """leave the title: Return twice - dosbox-x may eat the first; a second one is harmless in-game"""
        self.tap("Return", 0.5); self.tap("Return", 0.8)
    def hold(self, keys, seconds):
        """press keys together for a while, like a player holding them"""
        self.focus()
        for k in keys: sh("xdotool", "keydown", k, env=self.env)
        time.sleep(seconds)
        for k in keys: sh("xdotool", "keyup", k, env=self.env)
        time.sleep(0.15)
    def close(self, keep=False):
        if keep:
            print(f"left running on DISPLAY={DISPLAY}; dosbox pid {self.box.pid}, Xvfb pid {self.xvfb.pid}"); return
        # SIGKILL, not SIGTERM: dosbox-x's shutdown path pops a "close?" notification on the real desktop
        self.box.kill()
        try: self.box.wait(3)
        except Exception: pass
        self.xvfb.terminate()
        try: self.xvfb.wait(3)
        except Exception: self.xvfb.kill()
        lock = f"/tmp/.X{DISPLAY[1:]}-lock"             # the next session reuses the display number
        for _ in range(50):
            if not os.path.exists(lock): break
            time.sleep(0.1)

TITLE_WAIT = 4.5     # credits 1.5 s + title cascade 2.4 s, measured from the throwaway key

def tour(cycles, keep):
    out = os.path.join(ROOT, "build", "dosshots")
    shutil.rmtree(out, ignore_errors=True)
    s = Session(out, cycles)
    try:
        time.sleep(0.8); s.shot("credits")
        time.sleep(TITLE_WAIT - 0.8); s.shot("title")
        s.start_game(); s.shot("start")
        s.hold(["w"], 1.2); s.shot("walk_forward")
        s.hold(["Right"], 0.7); s.shot("turn_right")
        s.hold(["w"], 1.0); s.shot("walk_2")
        s.hold(["space"], 0.3); time.sleep(0.1); s.shot("shoot")
        s.tap("F1", 0.4); s.shot("fps_overlay")
        s.hold(["a"], 0.8); s.shot("strafe_left")
        s.hold(["Left"], 1.0); s.hold(["w", "space"], 1.5); s.shot("run_and_gun")
        time.sleep(1.5); s.shot("later")
        s.tap("Escape", 2.5); s.shot("credits_roll")
        s.tap("Return", 1.5); s.shot("exit_text")
    finally:
        s.close(keep)
    log = os.path.join(EXE_DIR, "MAZE2.OUT")
    if os.path.exists(log):
        print("--- MAZE2.OUT ---"); print(open(log, errors="replace").read())
    print(f"{len(s.shots)} screenshots in {out}")

def photos(cycles, keep):
    """README screenshots: each entry = (name, MAZE2 args, actions after the title accepted a key)"""
    out = os.path.join(ROOT, "screenshots")
    plan = [
        ("title",    "",                  lambda s: None),
        ("spawn",    "",                  lambda s: time.sleep(1.0)),
        ("grunt",    "-at 3.5 5.5 0",     lambda s: time.sleep(1.0)),
        ("firefight","-at 3.5 5.5 0",     lambda s: (s.hold(["space"], 0.3), time.sleep(0.15))),
        ("hub",      "-at 10.5 9.5 90",   lambda s: time.sleep(1.0)),
        ("armory",   "-at 17.5 3.5 0",    lambda s: time.sleep(1.0)),
        ("fps",      "-at 5.5 3.5 0",     lambda s: (s.tap("F1", 0.6))),
    ]
    for name, args, act in plan:
        s = Session(os.path.join(ROOT, "build", "photos"), cycles, args, log="PHOTO.OUT")
        try:
            if name == "title":
                time.sleep(TITLE_WAIT); s.shot("title", numbered=False)
            else:
                time.sleep(TITLE_WAIT); s.start_game(); act(s); s.shot(name, numbered=False)
        finally:
            s.close(keep and name == plan[-1][0])
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(os.path.join(ROOT, "build", "photos")):
        if f.endswith(".png"): shutil.copy(os.path.join(ROOT, "build", "photos", f), os.path.join(out, f))
    print("README screenshots in", out)

def main():
    keep = "--keep" in sys.argv
    cycles = 30000
    if "--cycles" in sys.argv: cycles = int(sys.argv[sys.argv.index("--cycles") + 1])
    if not os.path.exists(os.path.join(EXE_DIR, "MAZE2.EXE")):
        print("build first: tools/build.sh"); return 1
    if "--photos" in sys.argv: photos(cycles, keep)
    else: tour(cycles, keep)
    return 0

if __name__ == "__main__":
    sys.exit(main())
