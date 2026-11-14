"""Run the HTML cascade checks with the Windows x64 debug extension."""

import argparse
from pathlib import Path
import os
import shutil
import subprocess
from tempfile import TemporaryDirectory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("godot", type=Path, help="path to the Godot console executable")
    parser.add_argument("--renderer", choices=("gl_compatibility", "forward_plus"))
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    library = repo / "addons/docrender/bin/docrender.windows.template_debug.x86_64.dll"
    if not library.is_file():
        parser.error("Build the Windows x64 template_debug target first.")
    if not args.godot.is_file():
        parser.error("Godot executable does not exist.")

    with TemporaryDirectory(prefix="docrender-css-cascade-") as test_dir:
        project = Path(test_dir)
        extension = project / "addons/docrender"
        (extension / "bin").mkdir(parents=True)
        shutil.copyfile(library, extension / "bin" / library.name)
        shutil.copyfile(repo / "addons/docrender/docrender.gdextension", extension / "docrender.gdextension")
        (project / ".godot").mkdir()
        (project / ".godot/extension_list.cfg").write_text(
            "res://addons/docrender/docrender.gdextension\n", encoding="utf-8", newline="\n",
        )
        (project / "project.godot").write_text(
            'config_version=5\n\n[application]\nconfig/name="DocRender CSS cascade test"\n'
            '\n[debug]\nfile_logging/enable_file_logging=false\n'
            '\n[rendering]\nshader_compiler/shader_cache/enabled=false\n',
            encoding="utf-8", newline="\n",
        )
        shutil.copyfile(Path(__file__).with_name("css_cascade.gd"), project / "css_cascade.gd")
        styles = project / "styles"
        (styles / "nested").mkdir(parents=True)
        (styles / "sheet.css").write_text(
            '@import "nested/outer.css";\n.from_link { width: 30px; }\n'
            '.import_order { width: 23px; }\n.after_link { width: 13px; }\n',
            encoding="utf-8", newline="\n",
        )
        (styles / "nested/outer.css").write_text('@import "inner.css";\n', encoding="utf-8", newline="\n")
        (styles / "nested/inner.css").write_text(
            ".order { width: 22px; } .import_order { width: 22px; } .unused { width: bogus; }\n",
            encoding="utf-8", newline="\n",
        )
        (styles / "additional.css").write_text(".order { width: 31px; }\n", encoding="utf-8", newline="\n")
        (styles / "media.css").write_text(".link_media { width: 29px; }\n", encoding="utf-8", newline="\n")
        (styles / "conditional.css").write_text("#conditional_import { width: 51px; }\n", encoding="utf-8", newline="\n")
        (styles / "rejected.css").write_text(
            "#conditional_import { width: bogus; width: 99px !important; }\n", encoding="utf-8", newline="\n",
        )
        # Keep the test process's logs and shader cache in its temporary project.
        environment = {**os.environ, "APPDATA": str(project)}
        renderers = (args.renderer,) if args.renderer else ("gl_compatibility", "forward_plus")
        for renderer in renderers:
            completed = subprocess.run(
                [str(args.godot.resolve()), "--path", str(project), "--rendering-method", renderer,
                 "--script", "css_cascade.gd"],
                env=environment, cwd=project, timeout=60, check=False,
            )
            if completed.returncode != 0:
                return completed.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
