---------------------------------
-- HEADER: Each Script needs this
---------------------------------
local info = debug.getinfo(1,'S');
script_path = info.source:match[[^@?(.*[\/])[^\/]-$]]
package.path = package.path .. ";" .. script_path .. "?/init.lua" .. ";" .. script_path .. "?.lua"

lib = require("lib")

---------------------------------
-- Upspring --run runscripts/convert.lua -- ../TA ../TA/objects3d/tllcenturion.3do
---------------------------------

---------------------------------
-- Actual code
---------------------------------
upspring.load_archive(arg[1])

local atlas = upspring.atlas();

local model = upspring.Model()
local ok = model:Load3DO(arg[2])

model:load_3do_textures(upspring.get_texture_handler())
model:add_textures_to_atlas(atlas)

local _dirname = lib.utils.dirname(arg[2])
local _fileName = lib.utils.basename(arg[2], lib.utils.get_suffix(arg[2]))

local _ddsOut = lib.utils.join_paths(lib.utils.join_paths(lib.utils.dirname(_dirname), "../unittextures"), _fileName .. "_tex1.dds")

print("-- Packing atlas")
atlas:pack();

print("-- Saving atlas to", _ddsOut)
atlas:save(_ddsOut)
print("-- Saved atlas")

model.root:Rotate180();

model:convert_to_atlas_s3o(atlas)

model.root:NormalizeNormals();

model:Remove3DOBase()

--model:Triangleize()

local _s3oOut = lib.utils.join_paths(_dirname, _fileName .. ".s3o")
local ok = model:SaveS3O(_s3oOut)
if ok then
    print("-- Stored S3O to: '" .. _s3oOut .. "'")
else
    print("-- Store failed")
end