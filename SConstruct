#!/usr/bin/env python
# taudplay-godot — the Taud replayer as a Godot 4 GDExtension.
#
#   scons                               debug build for this machine
#   scons target=template_release       release build
#   scons platform=windows              cross-build (see godot-cpp's docs)
#
# The library lands in demo/addons/taudplay/bin/; copy demo/addons/taudplay
# into a project to use it there.

env = SConscript("godot-cpp/SConstruct")

TAUD = "#thirdparty/taudplay-c"
OUT = "demo/addons/taudplay/bin"

# ── taudplay-c ──
# Built with its own flags: bit-exact output needs every multiply and add
# rounded on its own (no fused multiply-add, never fast-math), and 32-bit x86
# must not carry excess precision in x87 registers.
cenv = env.Clone()
cenv.Append(CPPPATH=[TAUD + "/include"])
if env.get("is_msvc", False):
    cenv.Append(CCFLAGS=["/fp:precise"])
else:
    cenv.Append(CFLAGS=["-std=c99"], CCFLAGS=["-ffp-contract=off", "-fno-fast-math"])
    if env["arch"] == "x86_32":
        cenv.Append(CCFLAGS=["-msse2", "-mfpmath=sse"])

taud_sources = [
    "jsmath", "tables", "inst", "samplemod", "sampler", "envelope", "filter", "fm",
    "spatial", "binaural", "hrir_sadie", "mastering", "trigger", "effects", "row",
    "tick", "mixer", "engine", "parse", "zstd", "inflate", "resample", "player",
]
# Objects go to build/, so the submodule's tree stays clean.
taud_objects = [
    cenv.SharedObject(
        "build/taudplay-c/{}{}{}".format(name, env["suffix"], env["SHOBJSUFFIX"]), "{}/src/{}.c".format(TAUD, name)
    )
    for name in taud_sources
]

# ── the extension ──
env.Append(CPPPATH=["#src", TAUD + "/include"])
sources = Glob("src/*.cpp") + taud_objects

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "{0}/libtaudplay.{1}.{2}.framework/libtaudplay.{1}.{2}".format(OUT, env["platform"], env["target"]),
        source=sources,
    )
elif env["platform"] == "ios":
    library = env.StaticLibrary(
        "{}/libtaudplay.{}.{}{}.a".format(
            OUT, env["platform"], env["target"], ".simulator" if env["ios_simulator"] else ""
        ),
        source=sources,
    )
else:
    library = env.SharedLibrary("{}/libtaudplay{}{}".format(OUT, env["suffix"], env["SHLIBSUFFIX"]), source=sources)

env.NoCache(library)
Default(library)
