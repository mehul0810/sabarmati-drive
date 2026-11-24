"""Import the original rigid hatch chassis and build its Unreal Physics Asset."""
import os
import unreal as ue


folder = "/Game/Sabarmati/Riverfront/Vehicles/Physics"
library = ue.EditorAssetLibrary
if not library.does_asset_exist(folder + "/SK_HatchPhysics"):
    manager = ue.InterchangeManager.get_interchange_manager_scripted()
    pipeline = ue.InterchangeGenericAssetsPipeline()
    pipeline.get_editor_property("material_pipeline").set_editor_property("import_materials", False)
    meshes = pipeline.get_editor_property("mesh_pipeline")
    meshes.set_editor_property("import_static_meshes", False)
    meshes.set_editor_property("create_physics_asset", True)
    options = ue.ImportAssetParameters()
    options.set_editor_property("is_automated", True)
    options.set_editor_property("override_pipelines", [ue.SoftObjectPath(pipeline.get_path_name())])
    source = manager.create_source_data(os.environ["SABARMATI_HATCH_PHYSICS_SOURCE"])
    if not manager.import_asset(folder, source, options):
        raise RuntimeError("Could not import the hatch body and axle skeleton")

chassis = library.load_asset(folder + "/SK_HatchPhysics")
if not isinstance(chassis, ue.SkeletalMesh) or not chassis.get_editor_property("physics_asset"):
    raise RuntimeError("No Physics Asset was built for the hatch chassis")

# Auto-generated capsules use the chassis width as their radius. A 4 m body
# with that collision sits above the reach of its suspension even on flat roads.
physics = chassis.get_editor_property("physics_asset")
bodies = [body for body in ue.ObjectIterator(ue.BodySetup) if body.get_outer() == physics]
if len(bodies) != 1:
    raise RuntimeError("The hatch should contain one simulated chassis body")
body = bodies[0]
geometry = body.get_editor_property("agg_geom")
box = ue.KBoxElem()
for property_name, value in dict(center=ue.Vector(0, 0, 2), x=408., y=182., z=80.).items():
    box.set_editor_property(property_name, value)
geometry.set_editor_property("sphyl_elems", [])
geometry.set_editor_property("box_elems", [box])
body.set_editor_property("agg_geom", geometry)
if not library.save_directory(folder, only_if_is_dirty=False):
    raise RuntimeError("Could not save the chassis and its collision body")
ue.log("SABARMATI HATCH PHYSICS ASSETS SAVED")
