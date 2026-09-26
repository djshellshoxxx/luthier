/*  installer.md 11 / 5.2: content packages and delta patches. */

#include "TestFramework.h"

#include "../Updates/ContentPackage.h"

using namespace luthier;
using namespace luthier::tests;

namespace
{
    struct Keys
    {
        juce::RSAKey publicKey, privateKey;

        explicit Keys (int salt = 0)
        {
            const int seeds[] = { 11 + salt, 23, 57 * (salt + 1), 91, 1234567 + salt };
            juce::RSAKey::createKeyPair (publicKey, privateKey, 512, seeds, 5);
        }
    };

    const Keys& keys()
    {
        static Keys k;
        return k;
    }

    juce::MemoryBlock bytes (const juce::String& text) { return { text.toRawUTF8(), text.getNumBytesAsUTF8() }; }

    struct Scratch
    {
        juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("luthier-content", {});
        Scratch() { dir.createDirectory(); }
        ~Scratch() { dir.deleteRecursively(); }
        juce::File root() const { return dir.getChildFile ("ContentUpdates"); }
    };

    /** A zip written by hand, so a test can put in exactly what a hostile or
        broken package would. The manifest is signed with `signer`. */
    void writeRawPackage (const juce::File& file, const juce::var& manifest,
                          const std::map<juce::String, juce::MemoryBlock>& entries, const juce::RSAKey& signer)
    {
        const auto json = juce::JSON::toString (manifest);
        const auto jsonBytes = bytes (json);
        const auto sig = ContentPackage::signManifest (jsonBytes, signer);

        juce::ZipFile::Builder zip;
        zip.addEntry (new juce::MemoryInputStream (jsonBytes, true), 9, "manifest.json", {});
        zip.addEntry (new juce::MemoryInputStream (sig.toRawUTF8(), sig.getNumBytesAsUTF8(), true), 9, "manifest.sig", {});

        for (const auto& [path, data] : entries)
            zip.addEntry (new juce::MemoryInputStream (data, true), 9, path, {});

        file.deleteFile();
        juce::FileOutputStream out (file);
        zip.writeToStream (out, nullptr);
    }

    juce::var manifestFor (const juce::String& name, const std::map<juce::String, juce::MemoryBlock>& files,
                           const juce::String& hashOverride = {})
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("magic", ContentPackage::kMagic);
        m->setProperty ("schema", 1);
        m->setProperty ("name", name);
        m->setProperty ("version", "1");

        juce::Array<juce::var> list;

        for (const auto& [path, data] : files)
        {
            auto* e = new juce::DynamicObject();
            e->setProperty ("path", path);
            e->setProperty ("sha256", hashOverride.isNotEmpty() ? hashOverride : juce::SHA256 (data.getData(), data.getSize()).toHexString());
            list.add (juce::var (e));
        }

        m->setProperty ("files", list);
        return juce::var (m);
    }
}

//==============================================================================
LUTHIER_TEST (ContentPackage, aSignedPackageInstallsIntoItsFolder)
{
    Scratch s;
    const auto pkg = s.dir.getChildFile ("Blues Pack.luthiercontent");

    const std::map<juce::String, juce::MemoryBlock> files {
        { "Presets/Blues/Delta.luthierpreset", bytes ("{\"magic\":\"luthier.preset\"}") },
        { "Tunes/Twelve Bar.luthiertune", bytes ("{}") },
    };

    CHECK (ContentPackage::build (pkg, "BluesPack", "1.0", files, keys().privateKey));

    const auto r = ContentPackage::apply (pkg, keys().publicKey, s.root());
    CHECK_MSG (r.ok(), r.message);
    CHECK (r.installedTo == s.root().getChildFile ("BluesPack"));
    CHECK (s.root().getChildFile ("BluesPack/Presets/Blues/Delta.luthierpreset").loadFileAsString().contains ("luthier.preset"));
    CHECK (s.root().getChildFile ("BluesPack/.version").loadFileAsString() == "1.0");

    // No staging or backup left behind.
    int stray = 0;
    for (const auto& e : juce::RangedDirectoryIterator (s.root(), false, "*", juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles))
        stray += e.getFile().getFileName() != "BluesPack" ? 1 : 0;
    CHECK (stray == 0);
    CHECK (! s.root().getChildFile (".staging-BluesPack").exists() && ! s.root().getChildFile (".BluesPack.previous").exists());
}

LUTHIER_TEST (ContentPackage, aBadOrMissingSignatureIsRefused)
{
    Scratch s;
    const std::map<juce::String, juce::MemoryBlock> files { { "Presets/A.luthierpreset", bytes ("{}") } };

    // Signed with a different key.
    Keys other (99);
    const auto forged = s.dir.getChildFile ("forged.luthiercontent");
    CHECK (ContentPackage::build (forged, "Pack", "1", files, other.privateKey));

    auto r = ContentPackage::apply (forged, keys().publicKey, s.root());
    CHECK (r.status == ContentPackage::Status::badSignature);
    CHECK (! s.root().getChildFile ("Pack").exists());

    // Signed, then the manifest edited.
    const auto tampered = s.dir.getChildFile ("tampered.luthiercontent");
    writeRawPackage (tampered, manifestFor ("Pack", files), files, keys().privateKey);
    {
        juce::ZipFile zip (tampered);
        juce::MemoryBlock sig;
        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (zip.getIndexOfFileName ("manifest.sig")));
        in->readIntoMemoryBlock (sig);

        auto edited = manifestFor ("Pack", files);
        edited.getDynamicObject()->setProperty ("version", "2");

        juce::ZipFile::Builder b;
        const auto json = juce::JSON::toString (edited);
        b.addEntry (new juce::MemoryInputStream (json.toRawUTF8(), json.getNumBytesAsUTF8(), true), 9, "manifest.json", {});
        b.addEntry (new juce::MemoryInputStream (sig, true), 9, "manifest.sig", {});
        b.addEntry (new juce::MemoryInputStream (files.begin()->second, true), 9, files.begin()->first, {});
        tampered.deleteFile();
        juce::FileOutputStream out (tampered);
        b.writeToStream (out, nullptr);
    }

    r = ContentPackage::apply (tampered, keys().publicKey, s.root());
    CHECK (r.status == ContentPackage::Status::badSignature);

    // No signature at all.
    const auto unsigned_ = s.dir.getChildFile ("unsigned.luthiercontent");
    {
        juce::ZipFile::Builder b;
        const auto json = juce::JSON::toString (manifestFor ("Pack", files));
        b.addEntry (new juce::MemoryInputStream (json.toRawUTF8(), json.getNumBytesAsUTF8(), true), 9, "manifest.json", {});
        juce::FileOutputStream out (unsigned_);
        b.writeToStream (out, nullptr);
    }

    CHECK (ContentPackage::apply (unsigned_, keys().publicKey, s.root()).status == ContentPackage::Status::badSignature);

    // Not a zip.
    const auto junk = s.dir.getChildFile ("junk.luthiercontent");
    junk.replaceWithText ("hello");
    CHECK (ContentPackage::apply (junk, keys().publicKey, s.root()).status == ContentPackage::Status::notAPackage);
}

LUTHIER_TEST (ContentPackage, pathTraversalAndCodeAreRefused)
{
    CHECK (ContentPackage::isSafeRelativePath ("Presets/A.luthierpreset"));
    CHECK (! ContentPackage::isSafeRelativePath ("../evil.luthierpreset"));
    CHECK (! ContentPackage::isSafeRelativePath ("Presets/../../evil.luthierpreset"));
    CHECK (! ContentPackage::isSafeRelativePath ("/etc/passwd"));
    CHECK (! ContentPackage::isSafeRelativePath ("C:/Windows/x.json"));
    CHECK (! ContentPackage::isSafeRelativePath ("Presets\\..\\x.json"));
    CHECK (! ContentPackage::isSafeRelativePath ("Presets//x.json"));
    CHECK (! ContentPackage::isSafeRelativePath (""));

    CHECK (ContentPackage::isAllowedDataFile ("BodyIRs/x.wav"));
    CHECK (ContentPackage::isAllowedDataFile ("Guitars/migration.json"));
    CHECK (! ContentPackage::isAllowedDataFile ("lib/Luthier.so"));
    CHECK (! ContentPackage::isAllowedDataFile ("install.sh"));
    CHECK (! ContentPackage::isAllowedDataFile ("Luthier.exe"));
    CHECK (! ContentPackage::isAllowedDataFile ("plugin.dylib"));

    Scratch s;
    const auto outside = s.dir.getChildFile ("escaped.luthierpreset");

    // Signed and well-formed, but a path climbs out.
    {
        const std::map<juce::String, juce::MemoryBlock> files { { "../escaped.luthierpreset", bytes ("{}") } };
        const auto pkg = s.dir.getChildFile ("climb.luthiercontent");
        writeRawPackage (pkg, manifestFor ("Pack", files), files, keys().privateKey);

        const auto r = ContentPackage::apply (pkg, keys().publicKey, s.root());
        CHECK (r.status == ContentPackage::Status::unsafePath);
        CHECK (! outside.exists());
    }

    // An entry the manifest does not mention still has to be safe.
    {
        const std::map<juce::String, juce::MemoryBlock> listed { { "Presets/A.luthierpreset", bytes ("{}") } };
        auto entries = listed;
        entries["../../sneaky.json"] = bytes ("{}");
        const auto pkg = s.dir.getChildFile ("sneak.luthiercontent");
        writeRawPackage (pkg, manifestFor ("Pack", listed), entries, keys().privateKey);

        CHECK (ContentPackage::apply (pkg, keys().publicKey, s.root()).status == ContentPackage::Status::unsafePath);
    }

    // Code is refused however it is signed.
    {
        const std::map<juce::String, juce::MemoryBlock> files { { "bin/luthier", bytes ("ELF") }, { "run.sh", bytes ("#!/bin/sh") } };
        const auto pkg = s.dir.getChildFile ("code.luthiercontent");
        writeRawPackage (pkg, manifestFor ("Pack", files), files, keys().privateKey);

        CHECK (ContentPackage::apply (pkg, keys().publicKey, s.root()).status == ContentPackage::Status::notData);
    }

    // A package name that is a path.
    {
        const std::map<juce::String, juce::MemoryBlock> files { { "Presets/A.luthierpreset", bytes ("{}") } };
        const auto pkg = s.dir.getChildFile ("name.luthiercontent");
        writeRawPackage (pkg, manifestFor ("../Presets", files), files, keys().privateKey);

        CHECK (ContentPackage::apply (pkg, keys().publicKey, s.root()).status == ContentPackage::Status::unsafePath);
    }

    CHECK (! s.root().getChildFile ("Pack").exists());
}

LUTHIER_TEST (ContentPackage, aBadHashRollsBackAndOffersTheFullDownload)
{
    Scratch s;

    // Version 1, installed.
    const std::map<juce::String, juce::MemoryBlock> v1 {
        { "Presets/A.luthierpreset", bytes ("A1") },
        { "Presets/B.luthierpreset", bytes ("B1") },
    };
    const auto pkg = s.dir.getChildFile ("v1.luthiercontent");
    CHECK (ContentPackage::build (pkg, "Pack", "1", v1, keys().privateKey));
    CHECK (ContentPackage::apply (pkg, keys().publicKey, s.root()).ok());

    const auto installed = s.root().getChildFile ("Pack");

    // A full package whose file does not match its (signed) manifest hash.
    {
        const std::map<juce::String, juce::MemoryBlock> v2 { { "Presets/A.luthierpreset", bytes ("A2") } };
        const auto bad = s.dir.getChildFile ("bad.luthiercontent");
        writeRawPackage (bad, manifestFor ("Pack", v2, juce::String::repeatedString ("0", 64)), v2, keys().privateKey);

        const auto r = ContentPackage::apply (bad, keys().publicKey, s.root());
        CHECK (r.status == ContentPackage::Status::hashMismatch);
        CHECK (r.shouldOfferFullDownload());
        CHECK (installed.getChildFile ("Presets/A.luthierpreset").loadFileAsString() == "A1");
    }

    // A delta 1 -> 2 that replaces A, deletes B - but whose A does not verify.
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("magic", ContentPackage::kMagic);
        m->setProperty ("name", "Pack");
        m->setProperty ("version", "2");
        m->setProperty ("kind", "delta");
        m->setProperty ("from", "1");

        juce::Array<juce::var> list;
        auto* a = new juce::DynamicObject();
        a->setProperty ("path", "Presets/A.luthierpreset");
        a->setProperty ("sha256", juce::String::repeatedString ("f", 64));
        list.add (juce::var (a));
        auto* b = new juce::DynamicObject();
        b->setProperty ("path", "Presets/B.luthierpreset");
        b->setProperty ("action", "delete");
        list.add (juce::var (b));
        m->setProperty ("files", list);

        const auto delta = s.dir.getChildFile ("delta-bad.luthiercontent");
        writeRawPackage (delta, juce::var (m), { { "Presets/A.luthierpreset", bytes ("A2") } }, keys().privateKey);

        const auto r = ContentPackage::applyDelta (delta, keys().publicKey, s.root());
        CHECK (r.status == ContentPackage::Status::hashMismatch);
        CHECK (r.shouldOfferFullDownload());

        // Rolled back: A and B as they were, still version 1.
        CHECK (installed.getChildFile ("Presets/A.luthierpreset").loadFileAsString() == "A1");
        CHECK (installed.getChildFile ("Presets/B.luthierpreset").loadFileAsString() == "B1");
        CHECK (installed.getChildFile (".version").loadFileAsString() == "1");
        CHECK (! s.root().getChildFile (".staging-Pack").exists());
    }

    // The same delta done right applies; a second go at it is the wrong base.
    {
        auto extra = new juce::DynamicObject();
        extra->setProperty ("kind", "delta");
        extra->setProperty ("from", "1");
        auto* actions = new juce::DynamicObject();
        actions->setProperty ("Presets/B.luthierpreset", "delete");
        extra->setProperty ("actions", juce::var (actions));

        const auto delta = s.dir.getChildFile ("delta-good.luthiercontent");
        CHECK (ContentPackage::build (delta, "Pack", "2",
                                      { { "Presets/A.luthierpreset", bytes ("A2") }, { "Presets/B.luthierpreset", bytes ("") } },
                                      keys().privateKey, juce::var (extra)));

        auto r = ContentPackage::applyDelta (delta, keys().publicKey, s.root());
        CHECK_MSG (r.ok(), r.message);
        CHECK (installed.getChildFile ("Presets/A.luthierpreset").loadFileAsString() == "A2");
        CHECK (! installed.getChildFile ("Presets/B.luthierpreset").exists());
        CHECK (installed.getChildFile (".version").loadFileAsString() == "2");

        r = ContentPackage::applyDelta (delta, keys().publicKey, s.root());
        CHECK (r.status == ContentPackage::Status::wrongBase);
    }
}
