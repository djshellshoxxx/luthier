#!/usr/bin/env python3
"""Isolated verification of the pinned CLAP host-write intent patch."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "patch_clap_host_intent.py"
SOURCE = Path(sys.argv.pop(1))
sys.path.insert(0, str(SCRIPT.parent))
import patch_clap_host_intent as patch

class PatchTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="luthier-clap-intent-")
        self.root = Path(self.temporary.name)
        self.target = self.root / patch.RELATIVE
        self.target.parent.mkdir(parents=True)
        original = (SOURCE / patch.RELATIVE).read_bytes()
        canonical = original.replace(b"\r\n", b"\n")
        if hashlib.sha256(canonical).hexdigest() == patch.AFTER_SHA256:
            canonical = canonical.replace(patch.AFTER, patch.BEFORE, 1)
            self.assertEqual(hashlib.sha256(canonical).hexdigest(), patch.BEFORE_SHA256)
            original = canonical.replace(b"\n", b"\r\n") if b"\r\n" in original else canonical
        else:
            self.assertEqual(hashlib.sha256(canonical).hexdigest(), patch.BEFORE_SHA256)
        self.original = original
        self.target.write_bytes(original)

    def tearDown(self):
        self.temporary.cleanup()

    def run_patch(self, *args):
        return subprocess.run([sys.executable, str(SCRIPT), str(self.root), *args], capture_output=True, text=True)

    def test_exact_patch_is_idempotent_and_preserves_licence(self):
        self.assertEqual(self.run_patch().returncode, 0)
        patched = self.target.read_bytes()
        self.assertEqual(self.run_patch().returncode, 0)
        self.assertEqual(self.run_patch("--check").returncode, 0)
        self.assertEqual(self.target.read_bytes(), patched)
        self.assertEqual(self.original.split(b"#include")[0], patched.split(b"#include")[0])

    def test_unknown_source_refused_without_change(self):
        self.target.write_bytes(self.original + b"\n// unknown upstream change\n")
        before = self.target.read_bytes()
        self.assertNotEqual(self.run_patch().returncode, 0)
        self.assertEqual(self.target.read_bytes(), before)

    def test_check_refuses_unpatched_without_change(self):
        self.assertNotEqual(self.run_patch("--check").returncode, 0)
        self.assertEqual(self.target.read_bytes(), self.original)

    def test_real_wrapper_function_preserves_same_value_intent(self):
        def run_function(source):
            begin = source.index("    void paramSetValueAndNotifyIfChanged(")
            opening = source.index("{", begin)
            depth = 1
            end = opening + 1
            while depth:
                depth += (source[end] == "{") - (source[end] == "}")
                end += 1
            function = source[begin:end]
            harness = r"""
#include <cassert>
#define JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE(...)
#define JUCE_END_IGNORE_WARNINGS_GCC_LIKE
namespace juce {
template <class T> struct ScopedValueSetter {
    T& ref; T old;
    ScopedValueSetter(T& r, T v):ref(r),old(r){ref=v;}
    ~ScopedValueSetter(){ref=old;}
};
}
struct Parameter {
    float value=0; int sets=0, notices=0; bool* suppress;
    float getValue(){return value;}
    void setValue(float v){value=v;++sets;}
    void sendValueChangedMessageToListeners(float v){
        assert(*suppress); assert(v==value); ++notices;
    }
};
struct JUCEParameterVariant {Parameter* processorParam;};
struct Wrapper {
    bool supressParameterChangeMessages=false;
    static float getNormalisedParameterValue(JUCEParameterVariant&,float v){return v;}
""" + function + r"""
};
int main(){
    Wrapper wrapper; Parameter p; p.suppress=&wrapper.supressParameterChangeMessages;
    JUCEParameterVariant variant{&p};
    wrapper.paramSetValueAndNotifyIfChanged(variant,0);
    if(p.notices!=1 || p.sets!=0 || p.value!=0 || wrapper.supressParameterChangeMessages) return 1;
    wrapper.paramSetValueAndNotifyIfChanged(variant,1);
    if(p.notices!=2 || p.sets!=1 || p.value!=1 || wrapper.supressParameterChangeMessages) return 2;
}
"""
            cpp = self.root / "wrapper-test.cpp"
            binary = self.root / "wrapper-test"
            cpp.write_text(harness)
            compiler = shutil.which("c++") or shutil.which("g++")
            self.assertIsNotNone(compiler, "A C++ compiler is required for the wrapper intent regression")
            subprocess.run([compiler, "-std=c++17", str(cpp), "-o", str(binary)], check=True)
            return subprocess.run([str(binary)], capture_output=True).returncode

        self.assertNotEqual(run_function(self.original.decode()), 0)
        self.assertEqual(self.run_patch().returncode, 0)
        self.assertEqual(run_function(self.target.read_text()), 0)

    def test_crlf_retained(self):
        self.target.write_bytes(self.original.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        self.assertEqual(self.run_patch().returncode, 0)
        patched = self.target.read_bytes()
        self.assertNotIn(b"\n", patched.replace(b"\r\n", b""))
        self.assertEqual(hashlib.sha256(patched.replace(b"\r\n", b"\n")).hexdigest(), patch.AFTER_SHA256)

if __name__ == "__main__":
    unittest.main()
