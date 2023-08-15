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
            error("-- Load failed: " .. v)
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
