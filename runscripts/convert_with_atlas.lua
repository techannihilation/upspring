---------------------------------
-- HEADER: Each Script needs this
---------------------------------
local info = debug.getinfo(1,'S');
script_path = info.source:match[[^@?(.*[\/])[^\/]-$]]
package.path = package.path .. ";" .. script_path .. "?/init.lua" .. ";" .. script_path .. "?.lua"

lib = require("lib")

---------------------------------
-- Actual code
---------------------------------

---------------------------------
-- Usage: ./Upspring -r runscripts/convert_with_atlas.lua -- ../TA/unittextures/arm.yaml ../TA/objects3d/arm/armcom.3do 
---------------------------------
local atlas = upspring.atlas()
if not atlas:load_yaml(arg[1]) then
    error("failed to load the atlas yaml")
    return
end

print("-- Converting: ", arg[2])

local model = upspring.Model()
local ok = model:Load3DO(arg[2])

model.root:Rotate180();

model:convert_to_atlas_s3o(atlas)

model:Remove3DOBase()
-- model:Triangleize()

-- model.root:NormalizeNormals();


-- model:Cleanup();

local _dirname = lib.utils.dirname(arg[2])
local _fileName = lib.utils.basename(arg[2], lib.utils.get_suffix(arg[2]))

local _s3oOut = lib.utils.join_paths(_dirname, _fileName .. ".s3o")
local ok = model:SaveS3O(_s3oOut)
if ok then
    print("-- Stored S3O to: '" .. _s3oOut .. "'")
else
    print("-- Store failed")
end