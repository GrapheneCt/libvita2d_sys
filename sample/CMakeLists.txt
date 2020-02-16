cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
endif()

project(libvita2d_sys_sample)

add_subdirectory(gxminternalforvsh)
add_subdirectory(libkernel)

include("${VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "libvita2d_sys Sample")
set(VITA_TITLEID  "LV2DSAMPL")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE}")

link_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
	)

add_executable(${PROJECT_NAME}
	src/main.c
	)

target_link_libraries(${PROJECT_NAME}
	vita2d_sys
	SceGxm_stub
	SceGxm_stub_weak
	SceLibKernel_stub_weak
	SceAppMgr_stub 
	ScePgf_stub 
	SceSysmodule_stub
    )

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME} UNSAFE)

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
    )
