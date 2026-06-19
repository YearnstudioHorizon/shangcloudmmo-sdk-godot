#!/usr/bin/env python
import os
import sys

libname = "shangcloud_mmo"
projectdir = "demo"

localEnv = Environment(tools=["default"], PLATFORM="")
customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]
opts = Variables(customs, ARGUMENTS)
opts.Update(localEnv)
Help(opts.GenerateHelpText(localEnv))
env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print("Error: godot-cpp submodule not initialized.")
    print("Run: git submodule update --init --recursive")
    sys.exit(1)

env = SConscript("godot-cpp/SConstruct", {"env": env, "customs": customs})

env.Append(CPPPATH=["src/"])

env.Append(CPPPATH=["thirdparty/mbedtls/include/"])
env.Append(CPPPATH=["thirdparty/mbedtls/library/"])

mbedtls_sources = Glob("thirdparty/mbedtls/library/*.c")
sources = Glob("src/*.cpp")

all_sources = sources + mbedtls_sources

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData(
            "src/gen/doc_data.gen.cpp",
            source=Glob("doc_classes/*.xml")
        )
        all_sources.append(doc_data)
    except AttributeError:
        print("Not including class reference (pre-4.3 baseline).")

suffix = env["suffix"].replace(".dev", "").replace(".universal", "")
lib_filename = "{}{}{}{}".format(
    env.subst("$SHLIBPREFIX"), libname, suffix, env.subst("$SHLIBSUFFIX")
)

library = env.SharedLibrary(
    "bin/{}/{}".format(env["platform"], lib_filename),
    source=all_sources,
)

copy = env.Install("{}/bin/{}/".format(projectdir, env["platform"]), library)
Default(library, copy)
