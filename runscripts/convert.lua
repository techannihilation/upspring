---------------------------------
-- HEADER: Each Script needs this
---------------------------------
local info = debug.getinfo(1,'S');
script_path = info.source:match[[^@?(.*[\/])[^\/]-$]]
package.path = package.path .. ";" .. script_path .. "?/init.lua" .. ";" .. script_path .. "?.lua"

lib = require("lib")

upspring.load_archive("/home/pcdummy/Projekte/techannihilation/TA.zip")

---------------------------------
-- Actual code
---------------------------------

lib.model.Convert3DOToS3O(arg[1])