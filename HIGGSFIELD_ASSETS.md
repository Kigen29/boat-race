# Higgsfield assets

Generated in your Higgsfield account on 11 November 2026. All four are in your
Higgsfield library, and `Scripts/fetch_assets.sh` downloads them into
`HiggsfieldAssets/`.

| File | What it is | Higgsfield model | Cost |
|---|---|---|---|
| `SM_Boat.glb` | Textured PBR 3D speedboat, 60,000-face limit | Tripo H3.1 Image to 3D | 9 credits |
| `boat_reference.png` | Studio image the boat model was built from | GPT Image 2.6 | free (unlimited allowance) |
| `T_Title.png` | Menu background, 16:9 | GPT Image 2.6 | free (unlimited allowance) |
| `T_WaterNormal.png` | Tileable water ripple normal map | GPT Image 2.6 | free (unlimited allowance) |

Credits before: 10. Credits after: 1.

Nobody has looked at these files yet: the workspace that generated them could
not download or display them. Open them (or your Higgsfield library) before you
build on them, and regenerate any that miss.

## Where each one ends up

`Scripts/import_assets.sh` imports them to `/Game/Higgsfield` under the names
the game looks for at startup:

| Unreal asset | Used for | If it is missing |
|---|---|---|
| `SM_Boat` | Every player's boat. Scaled to 7 m and aligned automatically. | Placeholder boat made of basic shapes |
| `M_Water` (built from `T_WaterNormal`) | The lake surface | Flat dark-blue water |
| `T_Title` | Menu background | Plain panel over the 3D scene |

If the boat faces backwards, set `CustomMeshYawOffset=180` in
`Config/DefaultGame.ini`. `CustomMeshZOffset` raises or lowers it in centimetres.

## Prompts used

**Boat reference image** (GPT Image 2.6, 1:1)

> Studio product photograph of one single modern 7-metre sport speedboat,
> isolated on a seamless pure white background. Three-quarter front view from
> slightly above, the entire boat visible from bow to stern including the full
> hull below the waterline, centred with white margin all around. Glossy white
> fibreglass deep-V hull with a dark navy blue stripe along the side, low tinted
> wraparound windscreen, grey upholstered seats, stainless steel rails, one
> black outboard engine mounted at the stern. Soft even diffuse studio lighting,
> no hard shadows, no water, no trailer, no stand, no people, no text, no logos,
> no brand names. Photorealistic, sharp focus, high detail.

**Boat model** (Tripo H3.1 Image to 3D): the reference image above, with
`texture` on, `pbr` on, `face_limit` 60000.

**Menu background** (GPT Image 2.6, 16:9)

> Photorealistic cinematic photograph of four sport speedboats racing side by
> side across open tropical sea at golden hour, white spray and long foaming
> wakes, camera low near the water surface, red and green race marker buoys in
> the distance, dramatic clouds lit by low sun, realistic ocean water with
> reflections, slight motion blur on the water. No text, no logos, no people in
> close-up.

**Water normal map** (GPT Image 2.6, 1:1)

> Seamless tileable tangent-space normal map texture for a game engine, showing
> gentle open-ocean water ripples seen from directly above. Standard normal map
> colours only: flat base colour RGB 128,128,255 lavender-blue with soft pink
> and cyan slopes forming small rounded wind ripples and a few broader swells,
> evenly distributed across the whole square with no focal point. No lighting,
> no shadows, no highlights, no foam, no objects, no text, no border. Left edge
> matches right edge and top matches bottom so it tiles perfectly.

## Replacing an asset

Generate a new one in Higgsfield, save it into `HiggsfieldAssets/` under the
same file name, and run `Scripts/import_assets.sh` again.

Image-to-3D model prices checked on the day, for the same reference image:
SAM 3 3D 1 credit, Tripo text-to-3D 5, Hunyuan 3D v3.1 text-to-3D 7,
Tripo H3.1 9, Hunyuan3D v3 15, Meshy 6 text-to-3D 25, Meshy image-to-3D 30,
Tripo P2 34, Meshy 7 38.
