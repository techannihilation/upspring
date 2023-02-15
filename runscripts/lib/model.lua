local model = {}

function model.Convert3DOToS3O(input)
    local _dirname = lib.utils.dirname(input)
    local _fileName = lib.utils.basename(input, lib.utils.get_suffix(input))

    print("Converting model '" .. input .. "':")

    local model = upspring.Model()
    local ok = model:Load3DO(input)
    if ok then
        print("-- Loaded the 3do")
    else
        error("load failed.")
        return;
    end

    upspring.TexturesToModel(model)

    local lfs = require('lfs')
    local _oldDir = lfs.currentdir()

    local _unittexturesDir = lib.utils.join_paths(_dirname, lib.utils.join_paths("..", "unittextures"))
    if lib.utils.is_directory(_unittexturesDir) then
        lfs.chdir(_unittexturesDir)
    end

    local _texOut =  _fileName .. "_tex1.dds"
    local _texOutPath = lib.utils.join_paths(lfs.currentdir(), _texOut)
    local ok = model:ConvertToS3O(_texOutPath, 512, 512)
    if ok then
        print("-- Converted to S3O, wrote textures to: '" .. _texOutPath .. "'")
    else
        error("convert failed.")
        return
    end
    lfs.chdir(_oldDir)

    -- print("-- Remove 3DO base")
    -- model:Remove3DOBase()
    -- print("-- Making all names lowercase")
    -- model:AllLowerCaseNames()

    print("-- Normalize normals")
    model.root:NormalizeNormals();

    -- print("-- Cleanup Model")
    -- model:Cleanup()

    local _s3oOut = lib.utils.join_paths(_dirname, _fileName .. ".s3o")
    local ok = model:SaveS3O(_s3oOut)
    if ok then
        print("-- Stored S3O to: '" .. _s3oOut .. "'")
    end

    print("Done")
end

return model