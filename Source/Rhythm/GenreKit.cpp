#include "GenreKit.h"

namespace luthier
{

//==============================================================================
namespace
{
    juce::var stringArrayToVar (const juce::StringArray& strings)
    {
        juce::Array<juce::var> items;

        for (const auto& s : strings)
            items.add (s);

        return { items };
    }

    juce::StringArray varToStringArray (const juce::var& state)
    {
        juce::StringArray result;

        if (const auto* items = state.getArray())
            for (const auto& item : *items)
                result.add (item.toString());

        return result;
    }

    double getDouble (const juce::DynamicObject& object, const char* key, double fallback)
    {
        return object.hasProperty (key) ? (double) object.getProperty (key) : fallback;
    }
}

//==============================================================================
juce::var GenreKit::toVar() const
{
    auto* object = new juce::DynamicObject();

    object->setProperty ("name", name);
    object->setProperty ("voicing_style", getVoicingStyleName (voicingStyle));
    object->setProperty ("voicing_density", voicingDensity);
    object->setProperty ("hand_position", handPositionHint);
    object->setProperty ("strum_patterns", stringArrayToVar (strumPatterns));
    object->setProperty ("fingerpick_patterns", stringArrayToVar (fingerpickPatterns));
    object->setProperty ("timing_ms", humanise.timingMs);
    object->setProperty ("velocity_pct", humanise.velocityPercent);
    object->setProperty ("miss_pct", humanise.missPercent);
    object->setProperty ("ghost_pct", humanise.ghostPercent);
    object->setProperty ("humanise_amount", humanise.amount);
    object->setProperty ("strum_duration_ms", strumDurationMs);
    object->setProperty ("strum_evenness", strumEvenness);
    object->setProperty ("preferred_preset", preferredPreset);
    object->setProperty ("tags", stringArrayToVar (tags));

    if (bassGrid.isNotEmpty())
        object->setProperty ("bass_grid", bassGrid);   // MODEL-GAPS

    return { object };
}

GenreKit GenreKit::fromVar (const juce::var& state)
{
    GenreKit kit;

    auto* object = state.getDynamicObject();

    if (object == nullptr)
        return kit;

    kit.name = object->getProperty ("name").toString();

    // The style is stored by name rather than by index so that inserting a style
    // into the enum later cannot silently repoint every kit on disk.
    const auto styleName = object->getProperty ("voicing_style").toString();

    for (int i = 0; i < (int) VoicingStyle::numStyles; ++i)
    {
        if (styleName.equalsIgnoreCase (getVoicingStyleName ((VoicingStyle) i)))
        {
            kit.voicingStyle = (VoicingStyle) i;
            break;
        }
    }

    kit.voicingDensity   = juce::jlimit (0.0, 100.0, getDouble (*object, "voicing_density", 100.0));
    kit.handPositionHint = juce::jlimit (0, 22, (int) getDouble (*object, "hand_position", 0.0));

    kit.strumPatterns      = varToStringArray (object->getProperty ("strum_patterns"));
    kit.fingerpickPatterns = varToStringArray (object->getProperty ("fingerpick_patterns"));

    kit.humanise.timingMs        = getDouble (*object, "timing_ms", 8.0);
    kit.humanise.velocityPercent = getDouble (*object, "velocity_pct", 12.0);
    kit.humanise.missPercent     = getDouble (*object, "miss_pct", 0.0);
    kit.humanise.ghostPercent    = getDouble (*object, "ghost_pct", 0.0);
    kit.humanise.amount          = getDouble (*object, "humanise_amount", 1.0);

    kit.strumDurationMs = juce::jlimit (1.0, 250.0, getDouble (*object, "strum_duration_ms", 22.0));
    kit.strumEvenness   = juce::jlimit (0.0, 1.0, getDouble (*object, "strum_evenness", 0.6));

    kit.preferredPreset = object->getProperty ("preferred_preset").toString();
    kit.tags            = varToStringArray (object->getProperty ("tags"));
    kit.bassGrid        = object->getProperty ("bass_grid").toString();   // MODEL-GAPS

    return kit;
}

bool GenreKit::loadFrom (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());

    if (parsed.getDynamicObject() == nullptr)
        return false;

    auto loaded = fromVar (parsed);

    if (! loaded.isValid())
        return false;

    *this = std::move (loaded);
    return true;
}

bool GenreKit::saveTo (const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (toVar(), false));
}

//==============================================================================
namespace
{
    /** Builds a kit from a compact description, so that the table below reads as
        a list of styles rather than as thirty lines of assignment each.

        `strums` and `picks` are comma-separated pattern names; `feel` is the
        four humanisation numbers a style wants, in the order timing sigma in
        milliseconds, velocity spread, miss chance, ghost chance. */
    GenreKit makeKit (const char* name,
                      VoicingStyle style, double density, int handPosition,
                      const char* strums, const char* picks,
                      std::array<double, 4> feel,
                      double strumDurationMs, double strumEvenness,
                      const char* preferredPreset,
                      const char* tags)
    {
        GenreKit kit;

        kit.name             = name;
        kit.voicingStyle     = style;
        kit.voicingDensity   = density;
        kit.handPositionHint = handPosition;

        kit.strumPatterns      = juce::StringArray::fromTokens (juce::String (strums), ",", "");
        kit.fingerpickPatterns = juce::StringArray::fromTokens (juce::String (picks), ",", "");

        kit.strumPatterns.trim();
        kit.strumPatterns.removeEmptyStrings();
        kit.fingerpickPatterns.trim();
        kit.fingerpickPatterns.removeEmptyStrings();

        kit.humanise.timingMs        = feel[0];
        kit.humanise.velocityPercent = feel[1];
        kit.humanise.missPercent     = feel[2];
        kit.humanise.ghostPercent    = feel[3];
        kit.humanise.amount          = 1.0;

        kit.strumDurationMs = strumDurationMs;
        kit.strumEvenness   = strumEvenness;
        kit.preferredPreset = preferredPreset;
        kit.tags            = juce::StringArray::fromTokens (juce::String (tags), " ", "");

        return kit;
    }
}

//==============================================================================
GenreKitLibrary::GenreKitLibrary()
{
    addFactoryKits();
}

void GenreKitLibrary::addFactoryKits()
{
    kits.clear();

    using V = VoicingStyle;

    // ---- country -------------------------------------------------------------------
    kits.push_back (makeKit ("Nashville Country", V::open, 85, 0,
                             "Nashville Strum,Country Boom Chick,Country Rake",
                             "Boom Chick,Travis Picking",
                             { 7.0, 10.0, 0.5, 6.0 }, 20.0, 0.62,
                             "T-Style Country Twang", "country acoustic"));

    kits.push_back (makeKit ("Modern Country", V::open, 80, 2,
                             "Modern Country,Nashville Strum,Country Boom Chick",
                             "Boom Chick",
                             { 5.0, 9.0, 0.3, 4.0 }, 18.0, 0.70,
                             "T-Style Country Twang", "country electric modern"));

    kits.push_back (makeKit ("Bluegrass Flatpick", V::open, 90, 0,
                             "Bluegrass Flatpick,Nashville Strum,Country Rake",
                             "Boom Chick",
                             { 5.0, 13.0, 0.4, 8.0 }, 16.0, 0.72,
                             "Jumbo Bluegrass", "bluegrass acoustic fast"));

    // ---- blues ---------------------------------------------------------------------
    kits.push_back (makeKit ("Delta Blues", V::open, 70, 0,
                             "Blues Shuffle,Country Rake",
                             "Delta Blues Thumb,Piedmont Blues",
                             { 12.0, 18.0, 1.5, 14.0 }, 26.0, 0.45,
                             "Blues Slide", "blues delta acoustic"));

    kits.push_back (makeKit ("Chicago Blues", V::barre, 65, 3,
                             "Chicago Shuffle,Blues Shuffle",
                             "Delta Blues Thumb",
                             { 10.0, 16.0, 1.0, 12.0 }, 22.0, 0.52,
                             "Single-Cut Crunch", "blues chicago electric"));

    kits.push_back (makeKit ("Modern Blues Rhythm", V::barre, 60, 5,
                             "Modern Blues Rhythm,Chicago Shuffle",
                             "Delta Blues Thumb",
                             { 8.0, 14.0, 0.6, 9.0 }, 20.0, 0.58,
                             "Single-Cut Crunch", "blues modern electric"));

    // ---- folk and fingerstyle ------------------------------------------------------
    kits.push_back (makeKit ("Folk Fingerstyle", V::open, 85, 0,
                             "Folk Down Up,Classic Strum",
                             "Fingerstyle Folk,Travis Picking,Modern Folk",
                             { 8.0, 11.0, 0.3, 4.0 }, 24.0, 0.60,
                             "Fingerstyle Folk", "folk acoustic fingerstyle"));

    kits.push_back (makeKit ("Contemporary Fingerstyle", V::wide, 95, 2,
                             "Ambient Swell",
                             "Contemporary Fingerstyle,Modern Folk,Post-Rock Arpeggio",
                             { 6.0, 9.0, 0.2, 3.0 }, 28.0, 0.66,
                             "Fingerstyle Folk", "folk contemporary fingerstyle"));

    kits.push_back (makeKit ("Piedmont", V::open, 75, 0,
                             "Blues Shuffle,Folk Down Up",
                             "Piedmont Blues,Travis Picking",
                             { 10.0, 15.0, 0.8, 11.0 }, 24.0, 0.50,
                             "Parlor Blues", "blues piedmont acoustic"));

    // ---- latin ---------------------------------------------------------------------
    kits.push_back (makeKit ("Bossa Nova", V::drop2, 70, 3,
                             "Bossa Comp,Latin Ballad",
                             "Classical Ascending",
                             { 6.0, 9.0, 0.2, 2.0 }, 26.0, 0.72,
                             "Nylon Classical", "latin bossa nylon"));

    kits.push_back (makeKit ("Samba", V::drop2, 75, 3,
                             "Samba Comp,Bossa Comp",
                             "Classical Ascending",
                             { 6.0, 12.0, 0.3, 5.0 }, 20.0, 0.68,
                             "Nylon Classical", "latin samba brazilian"));

    kits.push_back (makeKit ("Latin Ballad", V::open, 80, 2,
                             "Latin Ballad,Bossa Comp",
                             "Classical Ascending,Classical Return",
                             { 8.0, 8.0, 0.2, 2.0 }, 32.0, 0.74,
                             "Nylon Classical", "latin ballad soft nylon"));

    // ---- reggae --------------------------------------------------------------------
    kits.push_back (makeKit ("Reggae Skank", V::triad, 45, 5,
                             "Reggae Skank,Rocksteady Skank",
                             "",
                             { 5.0, 10.0, 0.4, 3.0 }, 12.0, 0.82,
                             "Clean Double-Cut Funk", "reggae offbeat skank"));

    kits.push_back (makeKit ("Rocksteady", V::triad, 50, 5,
                             "Rocksteady Skank,Reggae Skank",
                             "",
                             { 7.0, 11.0, 0.5, 4.0 }, 14.0, 0.78,
                             "Clean Double-Cut Funk", "reggae rocksteady offbeat"));

    kits.push_back (makeKit ("Dub", V::triad, 40, 5,
                             "Dub Skank,Reggae Skank",
                             "",
                             { 9.0, 12.0, 1.5, 3.0 }, 14.0, 0.75,
                             "Surf Reverb", "reggae dub sparse"));

    // ---- funk ----------------------------------------------------------------------
    kits.push_back (makeKit ("Funk 16th", V::triad, 50, 5,
                             "Funk Sixteenth,Funk Wah Rhythm",
                             "",
                             { 4.0, 14.0, 0.5, 10.0 }, 11.0, 0.85,
                             "Clean Double-Cut Funk", "funk sixteenth tight"));

    kits.push_back (makeKit ("Funk Wah Rhythm", V::triad, 45, 7,
                             "Funk Wah Rhythm,Funk Sixteenth",
                             "",
                             { 4.0, 15.0, 0.6, 12.0 }, 10.0, 0.86,
                             "Wah Funk Rhythm", "funk wah tight"));

    // ---- rock and metal ------------------------------------------------------------
    kits.push_back (makeKit ("Punk Downstroke", V::power, 25, 3,
                             "Punk Downstroke,Classic Strum",
                             "",
                             { 6.0, 8.0, 0.2, 2.0 }, 14.0, 0.88,
                             "Single-Cut Crunch", "punk electric fast power"));

    kits.push_back (makeKit ("Metal Chug", V::power, 20, 0,
                             "Metal Chug,Djent Grid",
                             "",
                             { 3.0, 6.0, 0.1, 1.0 }, 9.0, 0.92,
                             "Modern Metal Chug", "metal electric palm-mute power"));

    kits.push_back (makeKit ("Djent Palm-Mute Grid", V::power, 20, 0,
                             "Djent Grid,Metal Chug",
                             "",
                             { 2.0, 5.0, 0.0, 0.0 }, 8.0, 0.95,
                             "8-String Djent", "metal djent palm-mute power"));

    // ---- jazz ----------------------------------------------------------------------
    kits.push_back (makeKit ("Jazz Comping", V::shell, 45, 5,
                             "Freddie Green,La Pompe",
                             "",
                             { 7.0, 10.0, 0.4, 3.0 }, 16.0, 0.74,
                             "Jazz Hollowbody", "jazz comping shell"));

    kits.push_back (makeKit ("Gypsy Jazz La Pompe", V::drop3, 55, 5,
                             "La Pompe,Freddie Green",
                             "",
                             { 6.0, 12.0, 0.3, 5.0 }, 15.0, 0.80,
                             "Jazz Hollowbody", "jazz gypsy pompe"));

    // ---- flamenco ------------------------------------------------------------------
    kits.push_back (makeKit ("Flamenco Rasgueado", V::open, 90, 2,
                             "Flamenco Rasgueado,Flamenco Alzapua",
                             "Classical Return",
                             { 9.0, 17.0, 0.5, 10.0 }, 30.0, 0.44,
                             "Flamenco Rasgueado", "flamenco spanish nylon"));

    kits.push_back (makeKit ("Flamenco Alzapua", V::open, 70, 2,
                             "Flamenco Alzapua,Flamenco Rasgueado",
                             "Classical Return",
                             { 8.0, 15.0, 0.4, 8.0 }, 22.0, 0.55,
                             "Flamenco Rasgueado", "flamenco spanish alzapua"));

    // ---- classical -----------------------------------------------------------------
    kits.push_back (makeKit ("Classical Etude", V::wide, 95, 2,
                             "",
                             "Classical Etude,Classical Ascending,Classical Return",
                             { 5.0, 7.0, 0.1, 1.0 }, 30.0, 0.70,
                             "Nylon Classical", "classical nylon study"));

    kits.push_back (makeKit ("Classical Arpeggio", V::wide, 100, 0,
                             "",
                             "Classical Ascending,Classical Return,Classical Etude",
                             { 5.0, 7.0, 0.1, 1.0 }, 30.0, 0.72,
                             "Nylon Classical", "classical nylon arpeggio"));

    // ---- ambient -------------------------------------------------------------------
    kits.push_back (makeKit ("Ambient Swell", V::wide, 100, 5,
                             "Ambient Swell",
                             "Post-Rock Arpeggio",
                             { 14.0, 8.0, 0.2, 1.0 }, 60.0, 0.40,
                             "Ambient Swell", "ambient slow wide"));

    kits.push_back (makeKit ("Post-Rock Arpeggio", V::wide, 95, 7,
                             "Ambient Swell",
                             "Post-Rock Arpeggio,Contemporary Fingerstyle",
                             { 8.0, 9.0, 0.2, 2.0 }, 40.0, 0.58,
                             "Ambient Swell", "ambient post-rock arpeggio"));

    // ---- bass (bass-techniques 9, factory-content 1.4: "and the same for bass") ----
    // The kits the bass step grid plays; the strum pattern is what a guitar
    // loaded under the kit falls back to. MODEL-GAPS.
    auto bassKit = [] (const char* name, const char* grid, const char* strum, const char* preset, const char* tags,
                       std::array<double, 4> feel)
    {
        auto kit = makeKit (name, V::bass, 30, 0, strum, "", feel, 30.0, 0.85, preset, tags);
        kit.bassGrid = grid;
        return kit;
    };

    kits.push_back (bassKit ("Funk Slap Bass", BassStepGrid::getFactoryName (BassStepGrid::Factory::slapFunk),
                             "Classic Strum", "J-Style Fingerstyle", "bass funk slap", { 4.0, 10.0, 0.0, 0.0 }));
    kits.push_back (bassKit ("Fingerstyle Groove Bass", BassStepGrid::getFactoryName (BassStepGrid::Factory::fingerstyleGroove),
                             "Classic Strum", "J-Style Fingerstyle", "bass fingerstyle rock pop", { 6.0, 10.0, 0.0, 0.0 }));
    kits.push_back (bassKit ("Motown Thumb Bass", BassStepGrid::getFactoryName (BassStepGrid::Factory::motownThumb),
                             "Country Boom Chick", "P-Bass Flatwound", "bass soul motown", { 7.0, 8.0, 0.0, 0.0 }));
    kits.push_back (bassKit ("Root-Fifth Bass", BassStepGrid::getFactoryName (BassStepGrid::Factory::rootFifthWalk),
                             "Country Boom Chick", "P-Bass Flatwound", "bass country folk", { 6.0, 8.0, 0.0, 0.0 }));
}

//==============================================================================
juce::File GenreKitLibrary::getFactoryDirectory()
{
    return juce::File::getSpecialLocation (juce::File::currentApplicationFile)
             .getParentDirectory()
             .getChildFile ("Resources")
             .getChildFile ("Genres");
}

juce::File GenreKitLibrary::getUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
             .getChildFile ("Luthier")
             .getChildFile ("Genres");
}

void GenreKitLibrary::refresh()
{
    addFactoryKits();

    scanDirectory (getFactoryDirectory());
    scanDirectory (getUserDirectory());
}

void GenreKitLibrary::scanDirectory (const juce::File& directory)
{
    if (! directory.isDirectory())
        return;

    for (const auto& entry : juce::RangedDirectoryIterator (directory, false, "*.json"))
    {
        GenreKit kit;

        if (! kit.loadFrom (entry.getFile()))
            continue;

        // Same rule as the pattern library: a kit on disk replaces the built-in
        // one of the same name, which is how a factory kit gets edited without
        // the original being lost.
        const int existing = indexOf (kit.name);

        if (existing >= 0)
            kits[(size_t) existing] = kit;
        else
            kits.push_back (kit);
    }
}

const GenreKit& GenreKitLibrary::getKit (int index) const noexcept
{
    static const GenreKit empty;

    return juce::isPositiveAndBelow (index, (int) kits.size())
             ? kits[(size_t) index] : empty;
}

int GenreKitLibrary::indexOf (const juce::String& kitName) const
{
    for (size_t i = 0; i < kits.size(); ++i)
        if (kits[i].name == kitName)
            return (int) i;

    return -1;
}

juce::Array<int> GenreKitLibrary::findByTag (const juce::String& tag) const
{
    juce::Array<int> result;

    for (size_t i = 0; i < kits.size(); ++i)
        if (kits[i].tags.contains (tag, true))
            result.add ((int) i);

    return result;
}

juce::StringArray GenreKitLibrary::getNames() const
{
    juce::StringArray names;

    for (const auto& kit : kits)
        names.add (kit.name);

    return names;
}

bool GenreKitLibrary::save (const GenreKit& kit)
{
    if (! kit.isValid())
        return false;

    const int existing = indexOf (kit.name);

    if (existing >= 0)
        kits[(size_t) existing] = kit;
    else
        kits.push_back (kit);

    const auto file = getUserDirectory()
                        .getChildFile (juce::File::createLegalFileName (kit.name) + ".json");

    return kit.saveTo (file);
}

//==============================================================================
bool GenreKitLibrary::apply (const GenreKit& kit, RhythmEngine& engine,
                             const PatternLibrary& patterns)
{
    engine.setVoicingStyle (kit.voicingStyle);
    engine.setVoicingDensity (kit.voicingDensity);
    engine.setHandPositionHint (kit.handPositionHint);
    engine.setHumanise (kit.humanise);
    engine.setStrumDurationMs (kit.strumDurationMs);
    engine.setStrumEvenness (kit.strumEvenness);

    // strum-dynamics 6.3: a kit is written for Feel 0.5, which is where Easy
    // mode's knob lands when a kit sets the humanise amount to 1.
    engine.setStrumFeel (0.5);

    // bass-techniques 9 (MODEL-GAPS): a bass kit's grid; any other kit clears it.
    {
        BassStepGrid grid;

        for (int f = 0; f < (int) BassStepGrid::Factory::numFactory; ++f)
            if (kit.bassGrid == BassStepGrid::getFactoryName ((BassStepGrid::Factory) f))
                grid = BassStepGrid::factory ((BassStepGrid::Factory) f);

        engine.setBassGrid (grid);
    }

    // A kit selects its first strum pattern, or its first fingerpick pattern if
    // it is a fingerstyle kit with no strums at all.
    for (const auto* list : { &kit.strumPatterns, &kit.fingerpickPatterns })
    {
        for (const auto& patternName : *list)
        {
            const int index = patterns.indexOf (patternName);

            if (index >= 0)
            {
                engine.setPattern (patterns.getPattern (index));
                return true;
            }
        }
    }

    return false;
}

int GenreKitLibrary::chooseRandomPattern (const GenreKit& kit, const PatternLibrary& patterns,
                                          juce::Random& random)
{
    juce::Array<int> candidates;

    for (const auto* list : { &kit.strumPatterns, &kit.fingerpickPatterns })
        for (const auto& patternName : *list)
            if (const int index = patterns.indexOf (patternName); index >= 0)
                candidates.addIfNotAlreadyThere (index);

    if (candidates.isEmpty())
        return -1;

    return candidates[random.nextInt (candidates.size())];
}

} // namespace luthier
