"""Exercise factory-table publication with fake build and render commands."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("regen_normalization_factory.sh")


class FactoryRegenerationTests(unittest.TestCase):
    def run_regeneration(self, artifact_path, render_exit=0, build_exit=0):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "scripts").mkdir()
            script = root / "scripts" / SCRIPT.name
            shutil.copyfile(SCRIPT, script)
            resources = root / "Resources"
            resources.mkdir()
            destination = resources / "NormalizationFactory.json"
            destination.write_text("original table\n")
            renderer = root / "build" / "LuthierRender_artefacts" / artifact_path
            renderer.parent.mkdir(parents=True)
            renderer.write_text(
                '#!/bin/bash\n'
                'test "$1" = --calibrate-factory || exit 99\n'
                'printf "generated table\\n" > "$2"\n'
                f'exit {render_exit}\n'
            )
            renderer.chmod(0o755)
            commands = root / "commands"
            commands.mkdir()
            ninja = commands / "ninja"
            ninja.write_text(f"#!/bin/bash\nexit {build_exit}\n")
            ninja.chmod(0o755)
            env = dict(os.environ, PATH=str(commands) + os.pathsep + os.environ["PATH"])
            result = subprocess.run(
                ["bash", str(script)], env=env, capture_output=True, text=True, timeout=10
            )
            return result, destination.read_text(), sorted(p.name for p in resources.iterdir())

    def test_failed_renderer_preserves_reference_and_cleans_temporary_output(self):
        for artifact_path in ("Release/LuthierRender", "LuthierRender"):
            with self.subTest(artifact_path=artifact_path):
                result, text, files = self.run_regeneration(artifact_path, render_exit=2)
                self.assertEqual(result.returncode, 2, result.stderr)
                self.assertEqual(text, "original table\n")
                self.assertEqual(files, ["NormalizationFactory.json"])

    def test_success_publishes_generated_table_and_cleans_temporary_output(self):
        for artifact_path in ("Release/LuthierRender", "LuthierRender"):
            with self.subTest(artifact_path=artifact_path):
                result, text, files = self.run_regeneration(artifact_path)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(text, "generated table\n")
                self.assertEqual(files, ["NormalizationFactory.json"])

    def test_failed_build_preserves_reference(self):
        for artifact_path in ("Release/LuthierRender", "LuthierRender"):
            with self.subTest(artifact_path=artifact_path):
                result, text, files = self.run_regeneration(artifact_path, build_exit=3)
                self.assertEqual(result.returncode, 3, result.stderr)
                self.assertEqual(text, "original table\n")
                self.assertEqual(files, ["NormalizationFactory.json"])


if __name__ == "__main__":
    unittest.main()
