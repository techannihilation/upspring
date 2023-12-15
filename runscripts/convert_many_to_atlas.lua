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
upspring.load_archive(arg[1])

local atlas = upspring.atlas();

for i, v in ipairs(arg) do
    if i > 2 then
        print("-- Loading the 3do", v)
        local model = upspring.Model()
        local ok = model:Load3DO(v)
        
        if ok then
        else
            error("-- Load failed", v)
            return;
        end

        model:load_3do_textures(upspring.get_texture_handler())
        model:add_textures_to_atlas(atlas)
    end
end

print("-- Packing atlas")
atlas:pack();

print("-- Saving atlas to", arg[2])
atlas:save(arg[2])
print("-- Saved atlas")


for i, v in ipairs(arg) do
    if i > 2 then
        print("-- Converting the 3do", v)
        local model = upspring.Model()
        local ok = model:Load3DO(v)

        -- model:Remove3DOBase();       
        model.root:Rotate180();
        model:Remove3DOBase()

        model:convert_to_atlas_s3o(atlas)

        model.root:NormalizeNormals();

        -- model:Triangleize()

        local _dirname = lib.utils.dirname(v)
        local _fileName = lib.utils.basename(v, lib.utils.get_suffix(v))

        local _s3oOut = lib.utils.join_paths(_dirname, _fileName .. ".s3o")
        local ok = model:SaveS3O(_s3oOut)
        if ok then
            print("-- Stored S3O to: '" .. _s3oOut .. "'")
        else
            print("-- Store failed")
        end
    end
end