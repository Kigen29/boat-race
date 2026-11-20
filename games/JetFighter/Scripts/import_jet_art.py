"""Regenerate JetFighter's owned aircraft assets from original GLB sources.

Runs in the full editor so StaticMeshEditorSubsystem can disable Nanite and
build ordinary LODs. Never imports Sky Warriors or other game's assets.
"""
from pathlib import Path
import json
import unreal

project=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
source=project/'Art/Models'
assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
registry=unreal.AssetRegistryHelpers.get_asset_registry()
subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
results=[]
for name in ('JetKite','JetRook','JetManta'):
    filename=source/(name+'.glb')
    if not filename.is_file():raise RuntimeError('Missing original GLB: '+str(filename))
    folder='/Game/Aircraft/Import/'+name
    task=unreal.AssetImportTask();task.set_editor_property('filename',str(filename));task.set_editor_property('destination_path',folder)
    task.set_editor_property('automated',True);task.set_editor_property('save',True);task.set_editor_property('replace_existing',True)
    tools.import_asset_tasks([task]);registry.scan_paths_synchronous([folder],True)
    meshes=[assets.load_asset(str(x.package_name)) for x in registry.get_assets_by_path(folder,recursive=True) if str(x.asset_class_path.asset_name)=='StaticMesh']
    if not meshes:raise RuntimeError('No imported static mesh for '+name)
    mesh=max(meshes,key=lambda x:x.get_bounds().box_extent.x*x.get_bounds().box_extent.y*x.get_bounds().box_extent.z)
    target='/Game/Aircraft/SM_'+name
    if assets.does_asset_exist(target):assets.delete_asset(target)
    mesh=assets.duplicate_asset(mesh.get_path_name().split('.')[0],target)
    if not mesh:raise RuntimeError('Could not publish '+target)
    settings=mesh.get_editor_property('nanite_settings');settings.set_editor_property('enabled',False);subsystem.set_nanite_settings(mesh,settings,True)
    options=unreal.StaticMeshReductionOptions();options.set_editor_property('auto_compute_lod_screen_size',False)
    options.set_editor_property('reduction_settings',[
        unreal.StaticMeshReductionSettings(percent_triangles=1.0,screen_size=1.0),
        unreal.StaticMeshReductionSettings(percent_triangles=0.45,screen_size=0.28),
        unreal.StaticMeshReductionSettings(percent_triangles=0.15,screen_size=0.08)])
    subsystem.set_lods(mesh,options)
    assets.save_asset(target)
    lods=subsystem.get_lod_count(mesh)
    if lods<3:raise RuntimeError('LODs not built for '+name)
    results.append({'name':name,'asset':target,'lods':lods,'materials':len(mesh.get_editor_property('static_materials'))})
    unreal.log('JET ART READY: '+name+' / '+str(lods)+' LODs / Nanite disabled')
(project/'Saved').mkdir(exist_ok=True)
(project/'Saved/jet_art_import.json').write_text(json.dumps(results,indent=2)+'\n')
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'quit')
