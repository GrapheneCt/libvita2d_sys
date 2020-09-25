cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{DOLCESDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DOLCESDK}/share/dolce.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define DOLCESDK to point to your SDK path!")
  endif()
endif()

include("$ENV{DOLCESDK}/share/dolce.cmake" REQUIRED)

project(vita2d_sys)

add_executable(${PROJECT_NAME}
  source/vita2d.c
  source/heap.c
  source/utils.c
  source/vita2d_draw.c
  source/vita2d_texture.c
  source/texture_atlas.c
  source/int_htab.c
  source/bin_packing_2d.c
  source/vita2d_image_gxt.c
  source/vita2d_image_bmp.c
  source/vita2d_image_jpeg.c
  source/vita2d_image_gim.c
  source/vita2d_pgf.c
  source/vita2d_pvf.c
)

link_directories(${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -nostdlib -fno-builtin")

set_target_properties(${PROJECT_NAME}
  PROPERTIES LINK_FLAGS
  -nostartfiles
)

include_directories(
  include
)

target_link_libraries(${PROJECT_NAME}
  SceSysmem_stub_weak
  SceDisplay_stub_weak
  SceDisplayUser_stub_weak
  SceSysmodule_stub_weak
  SceIofilemgr_stub_weak
  SceLibKernel_stub_weak
  SceKernelThreadMgr_stub_weak
  SceKernelModulemgr_stub_weak
  SceAppMgr_stub_weak
  SceShellSvc_stub_weak
  SceGxm_stub_weak
  SceGxmInternalForVsh_stub_weak
  SceCommonDialog_stub_weak
  SceDbg_stub_weak
  SceProcessMgr_stub_weak
  SceKernelDmacMgr_stub
  SceFios2_stub_weak
  SceJpeg_stub_weak
  SceJpegArm_stub_weak
  ScePgf_stub_weak
  ScePvf_stub_weak
  SceLibm_stub_weak
  SceRtabi_stub_weak
  SceSharedFb_stub_weak
)

dolce_create_self(${PROJECT_NAME}.suprx ${PROJECT_NAME}
  CONFIG ${CMAKE_SOURCE_DIR}/exports.yml
  UNSAFE
)

dolce_create_stubs(${PROJECT_NAME}_stubs 
  ${PROJECT_NAME} 
  ${CMAKE_SOURCE_DIR}/exports.yml
  LIB vita2d_sys_stub
)

