// OWNER: WP1 (frozen interface I-2; changes only via errata request to the WP1 owner)
#pragma once

// One registration entry point per package (errata E14). PedalRegistry::registerAll()
// calls every function below, in this order (which is the catalog order of
// PedalRegistry::all()). Each function lives in the package-owned file named in
// the comment and calls PedalRegistry::add() once per model. WP1 ships empty
// bodies; no package edits PedalRegistry.cpp.

namespace luthier::reference
{

void registerGenericA();   // Generic/RegisterGenericA.cpp   WP8   boost, overdrive, fuzz, compressor
void registerGenericB();   // Generic/RegisterGenericB.cpp   WP9   chorus, phaser, tremolo, wah
void registerGenericC();   // Generic/RegisterGenericC.cpp   WP10  delay, reverb
void registerDriveA();     // Models/RegisterDriveA.cpp      WP11  DS-1, TS808, SD-1
void registerDriveB();     // Models/RegisterDriveB.cpp      WP12  Distortion+, RAT, Dyna Comp
void registerMuffs();      // Models/RegisterMuffs.cpp       WP13  Big Muff family
void registerModDelay();   // Models/RegisterModDelay.cpp    WP14  CE-2, CE-2W, Phase 90, DD-3T
void registerLoopers();    // Models/RegisterLoopers.cpp     WP15  Ditto, Nano Looper 360, RC-1
void registerRc5();        // Models/RegisterRc5.cpp         WP16  RC-5
void registerPrototypes(); // Models/RegisterPrototypes.cpp  WP2   prototype.ts808|ds1|phase90

} // namespace luthier::reference
