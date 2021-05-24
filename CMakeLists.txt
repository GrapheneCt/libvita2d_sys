cmake_minimum_required(VERSION 3.19)

set(CMAKE_TOOLCHAIN_FILE "$ENV{SCE_PSP2_SDK_DIR}/host_tools/build/cmake/psp2-snc-toolchain.cmake")

project(vita2d_sys LANGUAGES C)

include(VitaDevelopmentSuite)
set(CMAKE_C_STANDARD 99)

add_compile_options(
  -Xdiag=0 -Xquit=2 -O3
)

add_link_options(
  --no-required-files
  -s
)

include_directories(
  ${VDSUITE_USER_INCLUDE_DIRECTORIES}
  libvita2d_sys/include
)

link_directories(
  ${VDSUITE_LIBRARY_DIRECTORIES}
)

add_library(${PROJECT_NAME} STATIC
  libvita2d_sys/source/vita2d.c
  libvita2d_sys/source/heap.c
  libvita2d_sys/source/utils.c
  libvita2d_sys/source/vita2d_draw.c
  libvita2d_sys/source/vita2d_texture.c
  libvita2d_sys/source/texture_atlas.c
  libvita2d_sys/source/int_htab.c
  libvita2d_sys/source/bin_packing_2d.c
  libvita2d_sys/source/vita2d_image_gxt.c
  libvita2d_sys/source/vita2d_image_bmp.c
  libvita2d_sys/source/vita2d_image_jpeg.c
  libvita2d_sys/source/vita2d_image_gim.c
  libvita2d_sys/source/vita2d_image_png.c
  libvita2d_sys/source/vita2d_pgf.c
  libvita2d_sys/source/vita2d_pvf.c
)

add_library("lib${PROJECT_NAME}.suprx" SHARED
  libvita2d_sys/source/vita2d.c
  libvita2d_sys/source/heap.c
  libvita2d_sys/source/utils.c
  libvita2d_sys/source/vita2d_draw.c
  libvita2d_sys/source/vita2d_texture.c
  libvita2d_sys/source/texture_atlas.c
  libvita2d_sys/source/int_htab.c
  libvita2d_sys/source/bin_packing_2d.c
  libvita2d_sys/source/vita2d_image_gxt.c
  libvita2d_sys/source/vita2d_image_bmp.c
  libvita2d_sys/source/vita2d_image_jpeg.c
  libvita2d_sys/source/vita2d_image_gim.c
  libvita2d_sys/source/vita2d_image_png.c
  libvita2d_sys/source/vita2d_pgf.c
  libvita2d_sys/source/vita2d_pvf.c
)

target_compile_definitions("lib${PROJECT_NAME}.suprx" PUBLIC -DVITA2D_SYS_PRX)

target_link_libraries("lib${PROJECT_NAME}.suprx"
  ScePng
  SceDeflt
  SceFpu
  SceSysmem_stub_weak
  SceDisplay_stub_weak
  SceDisplayUser_stub_weak
  SceSysmodule_stub_weak
  SceIofilemgr_stub_weak
  SceAppMgrUser_stub_weak
  SceShellSvc_stub_weak
  SceGxm_stub_weak
  SceGxmInternal_stub_weak
  SceGxmInternalForVsh_stub_weak
  SceGxmInternalForTest_stub_weak
  SceGpuEs4UserForVsh_stub_weak
  SceCommonDialog_stub
  SceDbg_stub_weak
  SceFios2_stub_weak
  SceJpegUser_stub_weak
  SceJpegArm_stub_weak
  ScePgf_stub_weak
  ScePvf_stub_weak
  SceRtabi_stub_weak
  SceSharedFb_stub_weak
  SceThreadmgr_stub_weak
  SceModulemgr_stub_weak
  SceProcessmgr_stub_weak
  SceDmacmgr_stub_weak
  ${CMAKE_SOURCE_DIR}/libvita2d_sys/libSceGpuEs4User_v2ds_stub.a
)
