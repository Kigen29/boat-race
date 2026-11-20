#!/usr/bin/env bash
# Downloads the Higgsfield-generated assets for this game into HiggsfieldAssets/.
# They were generated in your Higgsfield account on 2026-11-11; the same files
# are also in your Higgsfield library. See HIGGSFIELD_ASSETS.md for the prompts.
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$PROJECT_DIR/HiggsfieldAssets"
BASE="https://d8j0ntlcm91z4.cloudfront.net/user_3Lx3cyzN8Du6AYsTXY3qZCtf2tI"
mkdir -p "$OUT"

fetch() {
	local name="$1" url="$2"
	if [ -s "$OUT/$name" ] && [ "${FORCE:-0}" != "1" ]; then
		echo "have      $name"
		return
	fi
	echo "download  $name"
	curl --fail --location --silent --show-error --output "$OUT/$name.part" "$url"
	mv "$OUT/$name.part" "$OUT/$name"
}

# 3D boat model (Tripo H3.1 image-to-3D, PBR textured GLB)
fetch SM_Boat.glb       "$BASE/hf_20261111_141457_e242c595-23ed-43df-b7ba-c2322d8f0bb7.glb"
# The studio image the model was built from (reference only, not imported)
fetch boat_reference.png "$BASE/hf_20261111_141223_9f1c5f3b-67c1-4a72-83ba-82308470533d.png"
# Menu background
fetch T_Title.png       "$BASE/hf_20261111_141144_22dfdde9-05bd-4cd6-ab38-af8bf8c5026d.png"
# Tileable water ripple normal map
fetch T_WaterNormal.png "$BASE/hf_20261111_141438_af1b61f7-5df6-4ffc-928e-49bd6d937e5f.png"

echo
ls -lh "$OUT"
echo
echo "Next:  Scripts/import_assets.sh"
