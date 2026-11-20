Higgsfield-generated assets are imported into this folder (Unreal path /Game/Higgsfield).
The game looks for these exact asset names at startup and uses them when present:

  SM_Boat   static mesh   replaces the placeholder boat
  M_Water   material      replaces the flat-colour water
  T_Title   texture       menu / lobby background

Scripts/fetch_assets.sh downloads the generated files and
Scripts/import_assets.sh imports them and builds M_Water.
