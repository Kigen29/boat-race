# Original JetFighter aircraft source

JetKite.glb, JetRook.glb and JetManta.glb are original procedural prototype
models authored for this repository by Scripts/make_jets.py. They do not
contain extracted Sky Warriors meshes, textures or trademarks.

From the repository root:

```bash
Scripts/make_jets.sh       # requires Blender; regenerate source GLBs/gallery
Scripts/import_jet_art.sh  # requires built UE editor; import committed GLBs
Scripts/jet_art_tour.sh    # verify all three in rendered flight
```

The generator is the editable source; output is metres, +X forward, +Z up.
Models/airframes.json records dimensions, triangle counts and material slots.
The gallery and optional .blend are generated in Saved/JetArt. The importer
creates stable /Game/Aircraft/SM_JetKite, SM_JetRook and SM_JetManta assets,
with three LODs and Nanite disabled. Unreal's imported .uasset files are not
committed; regenerate them on a fresh checkout before packaging.

These are recognisable prototype airframes, not finished production art.
Current materials use colour-coded semantic slots, not production PBR atlases;
there are 12 slots per mesh. Stores are visual; no firing/rail removal yet.
They share one flight profile. Production UVs, reduced material slots,
control-surface animation, sockets and damage treatment are planned in
[the art bible](../docs/vision/03_ART_BIBLE.md).
