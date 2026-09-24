#include "ContentPackage.h"

#include <map>

namespace luthier
{

namespace
{
    ContentPackage::Result fail (ContentPackage::Status status, const juce::String& message)
    {
        ContentPackage::Result r;
        r.status = status;
        r.message = message;
        return r;
    }

    juce::String sha256Hex (const void* data, size_t size)
    {
        return juce::SHA256 (data, size).toHexString();
    }

    juce::BigInteger hashAsNumber (const juce::MemoryBlock& data)
    {
        const auto digest = juce::SHA256 (data.getData(), data.getSize()).getRawData();
        juce::BigInteger n;
        n.loadFromMemoryBlock (digest);
        return n;
    }

    /** The zip, its parsed manifest and the manifest's bytes. */
    struct Opened
    {
        std::unique_ptr<juce::ZipFile> zip;
        juce::MemoryBlock manifestBytes;
        juce::var manifest;
        juce::String signature;
    };

    bool readEntry (juce::ZipFile& zip, const juce::String& name, juce::MemoryBlock& out)
    {
        const int index = zip.getIndexOfFileName (name);

        if (index < 0)
            return false;

        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (index));

        if (in == nullptr)
            return false;

        out.reset();
        in->readIntoMemoryBlock (out);
        return true;
    }

    ContentPackage::Result open (const juce::File& file, const juce::RSAKey& key, Opened& o)
    {
        if (! file.existsAsFile())
            return fail (ContentPackage::Status::notAPackage, file.getFileName() + " does not exist.");

        o.zip = std::make_unique<juce::ZipFile> (file);

        juce::MemoryBlock sig;

        if (o.zip->getNumEntries() == 0 || ! readEntry (*o.zip, "manifest.json", o.manifestBytes))
            return fail (ContentPackage::Status::notAPackage, file.getFileName() + " is not a Luthier content package.");

        // Signature first: nothing in an unsigned manifest is trusted, not even its paths.
        if (! readEntry (*o.zip, "manifest.sig", sig))
            return fail (ContentPackage::Status::badSignature, file.getFileName() + " is not signed.");

        o.signature = sig.toString().trim();

        if (! ContentPackage::verifyManifest (o.manifestBytes, o.signature, key))
            return fail (ContentPackage::Status::badSignature, file.getFileName() + "'s signature does not verify.");

        o.manifest = juce::JSON::parse (o.manifestBytes.toString());

        if (o.manifest.getProperty ("magic", {}).toString() != ContentPackage::kMagic
              || ! o.manifest.getProperty ("files", {}).isArray())
            return fail (ContentPackage::Status::notAPackage, file.getFileName() + " has no valid manifest.");

        const auto name = o.manifest.getProperty ("name", {}).toString();

        if (! ContentPackage::isSafeRelativePath (name) || name.containsChar ('/'))
            return fail (ContentPackage::Status::unsafePath, "the package name \"" + name + "\" is not a folder name.");

        // Every entry must be safe and data - including entries the manifest
        // does not mention, which are refused rather than ignored.
        for (int i = 0; i < o.zip->getNumEntries(); ++i)
        {
            const auto entry = o.zip->getEntry (i)->filename;

            if (entry == "manifest.json" || entry == "manifest.sig" || entry.endsWithChar ('/'))
                continue;

            if (! ContentPackage::isSafeRelativePath (entry))
                return fail (ContentPackage::Status::unsafePath, "\"" + entry + "\" could escape the content folder.");

            if (o.zip->getEntry (i)->isSymbolicLink)
                return fail (ContentPackage::Status::unsafePath, "\"" + entry + "\" is a link.");
        }

        for (const auto& f : *o.manifest.getProperty ("files", {}).getArray())
        {
            const auto path = f.getProperty ("path", {}).toString();

            if (! ContentPackage::isSafeRelativePath (path))
                return fail (ContentPackage::Status::unsafePath, "\"" + path + "\" could escape the content folder.");

            if (! ContentPackage::isAllowedDataFile (path))
                return fail (ContentPackage::Status::notData, "\"" + path + "\" is not a content file.");
        }

        ContentPackage::Result ok;
        ok.status = ContentPackage::Status::applied;
        return ok;
    }

    /** Swaps `staging` into `destination`, keeping the old one until done. */
    ContentPackage::Result commit (const juce::File& staging, const juce::File& destination)
    {
        const auto backup = destination.getSiblingFile ("." + destination.getFileName() + ".previous");
        backup.deleteRecursively();

        if (destination.exists() && ! destination.moveFileTo (backup))
        {
            staging.deleteRecursively();
            return fail (ContentPackage::Status::ioError, "could not move the installed content aside.");
        }

        if (! staging.moveFileTo (destination))
        {
            // Roll back.
            destination.deleteRecursively();

            if (backup.exists())
                backup.moveFileTo (destination);

            staging.deleteRecursively();
            return fail (ContentPackage::Status::ioError, "could not install the new content; the old content is back.");
        }

        backup.deleteRecursively();

        ContentPackage::Result r;
        r.status = ContentPackage::Status::applied;
        r.installedTo = destination;
        return r;
    }

    /** Copies a folder tree; false on any failure. */
    bool copyTree (const juce::File& from, const juce::File& to)
    {
        if (! from.isDirectory())
            return to.createDirectory().wasOk();

        return from.copyDirectoryTo (to);
    }
}

//==============================================================================
juce::File ContentPackage::getContentUpdatesFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier").getChildFile ("ContentUpdates");
}

bool ContentPackage::isSafeRelativePath (const juce::String& path)
{
    if (path.isEmpty() || path.startsWithChar ('/') || path.containsChar ('\\') || path.containsChar (':')
          || path.containsChar ('\0') || path.length() > 512)
        return false;

    for (const auto& part : juce::StringArray::fromTokens (path, "/", ""))
        if (part.isEmpty() || part == "." || part == "..")
            return false;

    return true;
}

bool ContentPackage::isAllowedDataFile (const juce::String& path)
{
    static const juce::StringArray allowed {
        ".luthierpreset", ".luthierguitar", ".luthiertune", ".luthierpart",
        ".luthierpattern", ".luthierkit", ".luthierset", ".midprofile",
        ".wav", ".json", ".txt", ".md"
    };

    return allowed.contains (path.fromLastOccurrenceOf (".", true, false).toLowerCase());
}

juce::String ContentPackage::signManifest (const juce::MemoryBlock& manifestJson, const juce::RSAKey& privateKey)
{
    auto n = hashAsNumber (manifestJson);

    if (! privateKey.applyToValue (n))
        return {};

    return n.toString (16);
}

bool ContentPackage::verifyManifest (const juce::MemoryBlock& manifestJson, const juce::String& signatureHex,
                                     const juce::RSAKey& publicKey)
{
    if (signatureHex.isEmpty() || ! publicKey.isValid()
          || ! signatureHex.containsOnly ("0123456789abcdefABCDEF"))
        return false;

    juce::BigInteger value;
    value.parseString (signatureHex, 16);

    if (value.isZero() || ! publicKey.applyToValue (value))
        return false;

    return value == hashAsNumber (manifestJson);
}

bool ContentPackage::build (const juce::File& destination, const juce::String& name, const juce::String& version,
                            const std::map<juce::String, juce::MemoryBlock>& files, const juce::RSAKey& privateKey,
                            const juce::var& extraManifest)
{
    auto* manifest = new juce::DynamicObject();
    manifest->setProperty ("magic", kMagic);
    manifest->setProperty ("schema", 1);
    manifest->setProperty ("name", name);
    manifest->setProperty ("version", version);

    if (auto* extra = extraManifest.getDynamicObject())
        for (const auto& p : extra->getProperties())
            manifest->setProperty (p.name, p.value);

    juce::Array<juce::var> list;
    juce::ZipFile::Builder zip;

    for (const auto& [path, bytes] : files)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("path", path);
        entry->setProperty ("sha256", sha256Hex (bytes.getData(), bytes.getSize()));
        entry->setProperty ("size", (juce::int64) bytes.getSize());

        // A delta may carry a "delete" with no bytes.
        if (auto* extra = extraManifest.getDynamicObject())
            if (auto* actions = extra->getProperty ("actions").getDynamicObject())
                if (actions->hasProperty (path))
                    entry->setProperty ("action", actions->getProperty (path));

        list.add (juce::var (entry));
        zip.addEntry (new juce::MemoryInputStream (bytes, true), 9, path, juce::Time());
    }

    manifest->setProperty ("files", list);

    const auto json = juce::JSON::toString (juce::var (manifest));
    juce::MemoryBlock jsonBytes (json.toRawUTF8(), json.getNumBytesAsUTF8());
    const auto sig = signManifest (jsonBytes, privateKey);

    zip.addEntry (new juce::MemoryInputStream (jsonBytes, true), 9, "manifest.json", juce::Time());
    zip.addEntry (new juce::MemoryInputStream (sig.toRawUTF8(), sig.getNumBytesAsUTF8(), true), 9, "manifest.sig", juce::Time());

    destination.deleteFile();
    juce::FileOutputStream out (destination);
    return out.openedOk() && zip.writeToStream (out, nullptr);
}

//==============================================================================
ContentPackage::Result ContentPackage::apply (const juce::File& packageFile, const juce::RSAKey& trustedKey,
                                              const juce::File& root)
{
    Opened o;

    if (auto r = open (packageFile, trustedKey, o); ! r.ok())
        return r;

    const auto name = o.manifest.getProperty ("name", {}).toString();
    const auto destination = root.getChildFile (name);
    const auto staging = root.getChildFile (".staging-" + name);

    staging.deleteRecursively();

    if (! staging.createDirectory().wasOk())
        return fail (Status::ioError, "could not create " + staging.getFullPathName());

    for (const auto& f : *o.manifest.getProperty ("files", {}).getArray())
    {
        const auto path = f.getProperty ("path", {}).toString();
        juce::MemoryBlock bytes;

        if (! readEntry (*o.zip, path, bytes)
              || sha256Hex (bytes.getData(), bytes.getSize()) != f.getProperty ("sha256", {}).toString())
        {
            staging.deleteRecursively();
            return fail (Status::hashMismatch, "\"" + path + "\" does not match the manifest.");
        }

        const auto target = staging.getChildFile (path);

        if (! target.isAChildOf (staging) || ! target.getParentDirectory().createDirectory().wasOk()
              || ! target.replaceWithData (bytes.getData(), bytes.getSize()))
        {
            staging.deleteRecursively();
            return fail (Status::ioError, "could not write \"" + path + "\".");
        }
    }

    // What was installed, so a later delta can check its base.
    staging.getChildFile (".version").replaceWithText (o.manifest.getProperty ("version", {}).toString());

    root.createDirectory();
    return commit (staging, destination);
}

ContentPackage::Result ContentPackage::applyDelta (const juce::File& patchFile, const juce::RSAKey& trustedKey,
                                                   const juce::File& root)
{
    Opened o;

    if (auto r = open (patchFile, trustedKey, o); ! r.ok())
        return r;

    if (o.manifest.getProperty ("kind", {}).toString() != "delta")
        return fail (Status::notAPackage, patchFile.getFileName() + " is not a delta patch.");

    const auto name = o.manifest.getProperty ("name", {}).toString();
    const auto destination = root.getChildFile (name);
    const auto installed = destination.getChildFile (".version").loadFileAsString().trim();
    const auto from = o.manifest.getProperty ("from", {}).toString();

    // 5.2: one-way, and it cannot skip versions.
    if (! destination.isDirectory() || installed != from)
        return fail (Status::wrongBase, "this patch updates " + name + " " + from + "; "
                                          + (installed.isEmpty() ? juce::String ("it is not installed") : "installed is " + installed) + ".");

    // Work on a copy; the installed folder is untouched until the swap.
    const auto staging = root.getChildFile (".staging-" + name);
    staging.deleteRecursively();

    if (! copyTree (destination, staging))
    {
        staging.deleteRecursively();
        return fail (Status::ioError, "could not stage " + name + ".");
    }

    for (const auto& f : *o.manifest.getProperty ("files", {}).getArray())
    {
        const auto path = f.getProperty ("path", {}).toString();
        const auto target = staging.getChildFile (path);

        if (! target.isAChildOf (staging))
        {
            staging.deleteRecursively();
            return fail (Status::unsafePath, "\"" + path + "\" could escape the content folder.");
        }

        if (f.getProperty ("action", "replace").toString() == "delete")
        {
            target.deleteFile();
            continue;
        }

        juce::MemoryBlock bytes;

        if (! readEntry (*o.zip, path, bytes)
              || ! target.getParentDirectory().createDirectory().wasOk()
              || ! target.replaceWithData (bytes.getData(), bytes.getSize()))
        {
            staging.deleteRecursively();
            return fail (Status::ioError, "could not write \"" + path + "\".");
        }

        // The file as it now is on disk, against the manifest.
        if (juce::SHA256 (target).toHexString() != f.getProperty ("sha256", {}).toString())
        {
            staging.deleteRecursively();
            return fail (Status::hashMismatch, "\"" + path + "\" did not verify after patching; nothing was changed. "
                                                 "Download the full content package instead.");
        }
    }

    staging.getChildFile (".version").replaceWithText (o.manifest.getProperty ("version", {}).toString());
    return commit (staging, destination);
}

} // namespace luthier
