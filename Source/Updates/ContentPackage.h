#pragma once

/*  installer.md 11 and 5.2: content updates and delta patches.

    A `.luthiercontent` file is a zip holding
        manifest.json   { "magic": "luthier.content", "schema": 1,
                          "name": "...", "version": "...",
                          "files": [ { "path": "...", "sha256": "...", "size": n } ] }
        manifest.sig    the manifest's SHA-256, signed with the release key
                        (RSA, hex; verified against the trusted public key)
        <the files the manifest lists>

    apply() refuses a package whose signature does not verify, whose manifest
    names a path that could leave the destination (absolute, "..", a drive,
    a backslash) or a file type outside the data-only allowlist ("content
    updates never modify code"), or whose files do not match their hashes. It
    unpacks into a staging folder beside the destination, verifies everything
    there, and only then swaps it into ContentUpdates/<name>/, keeping the
    previous version until the swap has succeeded - so a failure at any point
    leaves the installed content exactly as it was.

    A delta patch is the same container with `"kind": "delta"`, `"from"` (the
    version it applies to) and per-file actions ("replace" / "delete"); every
    resulting file is checked against its manifest hash, and any mismatch
    rolls the whole folder back ("failed apply rolls back and prompts full
    download").

    Message or worker thread (file I/O). No UI; Options -> Updates calls it.
*/

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

namespace luthier
{

class ContentPackage
{
public:
    static constexpr const char* kFileExtension = ".luthiercontent";
    static constexpr const char* kMagic = "luthier.content";

    enum class Status
    {
        applied = 0,
        notAPackage,        ///< not a zip, or no manifest
        badSignature,       ///< missing or does not verify
        unsafePath,         ///< a path that could escape the destination
        notData,            ///< a file type outside the allowlist
        hashMismatch,       ///< a file does not match the manifest
        wrongBase,          ///< a delta for a different installed version
        ioError             ///< the disk refused; nothing was changed
    };

    struct Result
    {
        Status status = Status::notAPackage;
        juce::String message;
        juce::File installedTo;

        bool ok() const noexcept { return status == Status::applied; }
        bool shouldOfferFullDownload() const noexcept { return status == Status::hashMismatch || status == Status::wrongBase; }
    };

    /** ~/Documents/Luthier/ContentUpdates. */
    static juce::File getContentUpdatesFolder();

    /** A full content package, into <root>/<manifest name>/. */
    static Result apply (const juce::File& packageFile, const juce::RSAKey& trustedKey,
                         const juce::File& contentUpdatesRoot = getContentUpdatesFolder());

    /** A delta patch, onto an installed <root>/<name>/ at version "from". */
    static Result applyDelta (const juce::File& patchFile, const juce::RSAKey& trustedKey,
                              const juce::File& contentUpdatesRoot = getContentUpdatesFolder());

    //==========================================================================
    // The pure checks, exposed for tests and for the builder below.

    /** A relative path with no way out: no leading '/', no drive, no '\\', no
        '..' or '.' component, no empty component. */
    static bool isSafeRelativePath (const juce::String& path);

    /** installer.md 11: presets, guitars, tunes, parts, IRs, translations and
        the guitar-migration table - data, never code. */
    static bool isAllowedDataFile (const juce::String& path);

    static juce::String signManifest (const juce::MemoryBlock& manifestJson, const juce::RSAKey& privateKey);
    static bool verifyManifest (const juce::MemoryBlock& manifestJson, const juce::String& signatureHex,
                                const juce::RSAKey& publicKey);

    /** Builds a package (the release tooling; also the tests' fixture).
        `files` maps package paths to their bytes. */
    static bool build (const juce::File& destination, const juce::String& name, const juce::String& version,
                       const std::map<juce::String, juce::MemoryBlock>& files, const juce::RSAKey& privateKey,
                       const juce::var& extraManifest = {});
};

} // namespace luthier
