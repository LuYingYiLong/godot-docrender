import os
from pathlib import Path
import subprocess
from tempfile import TemporaryDirectory

from SCons.Errors import UserError


def apply_litehtml_patch(target, source, env):
    upstream, patch = (Path(node.get_abspath()) for node in source)
    output = Path(target[0].get_abspath())
    relative = output.relative_to(env["LITEHTML_STAGED_DIR"])

    # Start with the pinned source each time so retries and parallel actions are independent.
    with TemporaryDirectory(dir=output.parent, prefix="litehtml-") as scratch:
        patched = Path(scratch, relative)
        patched.parent.mkdir(parents=True, exist_ok=True)
        original = upstream.read_text(encoding="utf-8")
        patched.write_text(original, encoding="utf-8", newline="\n")

        git_env = os.environ.copy()
        for variable in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE"):
            git_env.pop(variable, None)
        git_env["GIT_CEILING_DIRECTORIES"] = str(output.parent)
        try:
            result = subprocess.run(
                ["git", "-c", "core.autocrlf=false", "-c", "core.eol=lf", "apply", "--no-index",
                 "--whitespace=nowarn", "--no-ignore-whitespace", str(patch)],
                cwd=scratch, env=git_env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                encoding="utf-8", errors="replace", check=False,
            )
        except OSError as error:
            raise UserError(f"Could not run Git to apply {patch.name}: {error}") from error
        if result.returncode or not patched.is_file() or patched.read_bytes() == original.encode("utf-8"):
            detail = result.stderr.rstrip() or "The patch did not modify the expected file."
            raise UserError(f"Could not apply {patch.name} to litehtml {relative.as_posix()}.\n{detail}")
        patched.replace(output)


def build_litehtml(env, html_sources, enable_cpp_exceptions):
    litehtml = "src/thirdparty/litehtml"
    gumbo = f"{litehtml}/src/gumbo"
    staged_dir = "src/gen/html"
    staged_include = f"{staged_dir}/include"
    if not Path(litehtml, "include/litehtml.h").is_file() or not Path(gumbo, "include/gumbo.h").is_file():
        raise UserError("Missing litehtml or its bundled Gumbo. Run git submodule update --init --recursive.")

    patches = {}
    patch_root = Path(__file__).resolve().with_name("patches")
    for patch in sorted(patch_root.glob("*/**/*.patch")):
        relative = Path(*patch.relative_to(patch_root).parts[1:]).with_suffix("").as_posix()
        if relative in patches:
            raise UserError(f"Multiple litehtml patches for {relative}: {patches[relative]} and {patch}")
        patches[relative] = patch

    html_env = env.Clone(LITEHTML_STAGED_DIR=env.Dir(staged_dir).get_abspath())
    patched_files = []

    def stage_patch(source, relative):
        patch = patches.pop(relative, None)
        if patch is None:
            return None
        nodes = html_env.Command(f"{staged_dir}/{relative}", [source, html_env.File(str(patch))], apply_litehtml_patch)
        patched_files.extend(nodes)
        return nodes[0]

    staged_headers = []
    for header in html_env.Glob(f"{litehtml}/include/*.h") + html_env.Glob(f"{litehtml}/include/litehtml/*.h"):
        relative = Path(str(header)).relative_to(litehtml)
        staged = stage_patch(header, relative.as_posix())
        if staged is None:
            staged_headers += html_env.Install(f"{staged_dir}/{relative.parent.as_posix()}", header)
        else:
            staged_headers.append(staged)

    html_env.Append(CPPPATH=[staged_include, f"{staged_include}/litehtml", f"{litehtml}/src", f"{gumbo}/include"])
    # litehtml uses C++ exceptions during HTML encoding detection.
    enable_cpp_exceptions(html_env)
    if html_env.get("is_msvc", False):
        html_env.AppendUnique(CXXFLAGS=["/permissive-"])
    html_object = html_env.StaticObject if env["platform"] == "ios" else html_env.SharedObject
    html_objects = list(html_object(html_sources))

    for source in html_env.Glob(f"{litehtml}/src/*.cpp"):
        relative = Path(str(source)).relative_to(litehtml)
        staged = stage_patch(source, relative.as_posix())
        object_path = f"{staged_dir}/{relative.with_suffix('').as_posix()}{env['OBJSUFFIX']}"
        html_objects += html_object(object_path, source if staged is None else staged)

    if patches:
        raise UserError(f"litehtml patches have no matching upstream file: {', '.join(sorted(patches))}")
    html_env.Depends(patched_files, html_env.File(__file__))
    html_env.Depends(html_objects, staged_headers)

    gumbo_env = env.Clone()
    gumbo_env.Append(CPPPATH=[f"{gumbo}/include", f"{gumbo}/include/gumbo"])
    if gumbo_env.get("is_msvc", False):
        gumbo_env.Append(CPPPATH=[f"{gumbo}/visualc/include"])
        gumbo_env.AppendUnique(CCFLAGS=["/wd4244", "/wd4267"])
    else:
        gumbo_env.AppendUnique(CFLAGS=["-std=c99"])
    gumbo_object = gumbo_env.StaticObject if env["platform"] == "ios" else gumbo_env.SharedObject
    gumbo_objects = []
    for source in gumbo_env.Glob(f"{gumbo}/*.c"):
        relative = Path(str(source)).relative_to(litehtml).with_suffix("")
        gumbo_objects += gumbo_object(f"{staged_dir}/{relative.as_posix()}{env['OBJSUFFIX']}", source)
    return html_objects + gumbo_objects
