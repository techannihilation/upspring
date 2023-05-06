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
upspring.make_archive_atlas("/home/pcdummy/Projekte/techannihilation/TA.zip", "/home/pcdummy/Projekte/techannihilation/TA_Atlas.png", false)