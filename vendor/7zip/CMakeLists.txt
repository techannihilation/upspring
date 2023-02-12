project(7zip)

add_library(${PROJECT_NAME} STATIC
    7z/7zAlloc.c
    7z/7zArcIn.c
    7z/7zBuf.c
    7z/7zBuf2.c
    7z/7zCrc.c
    7z/7zCrcOpt.c
    7z/7zDec.c
    7z/7zFile.c
    7z/7zStream.c
    7z/Aes.c
    7z/AesOpt.c
    7z/Alloc.c
    7z/Bcj2.c
    7z/Bcj2Enc.c
    7z/Bra.c
    7z/Bra86.c
    7z/BraIA64.c
    7z/CpuArch.c
    7z/Delta.c
    7z/DllSecur.c
    7z/LzFind.c
    7z/LzFindMt.c
    7z/LzFindOpt.c
    7z/Lzma2Dec.c
    7z/Lzma2DecMt.c
    7z/Lzma2Enc.c
    7z/Lzma86Dec.c
    7z/Lzma86Enc.c
    7z/LzmaDec.c
    7z/LzmaEnc.c
    7z/LzmaLib.c
    7z/MtCoder.c
    7z/MtDec.c
    7z/Ppmd7.c
    7z/Ppmd7Dec.c
    7z/Ppmd7Enc.c
    7z/Sha256.c
    7z/Sha256Opt.c
    7z/Sort.c
    7z/Threads.c
    7z/Xz.c
    7z/XzCrc64.c
    7z/XzCrc64Opt.c
    7z/XzDec.c
    7z/XzEnc.c
    7z/XzIn.c
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
endif()

target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}" )
