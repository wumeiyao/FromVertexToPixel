find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

set(CODEA_EXERCISE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/exercise_output")

function(codea_console_exercise target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE glm::glm)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    target_compile_definitions(${target} PRIVATE
        CODEA_OUTPUT_DIR="${CODEA_EXERCISE_OUTPUT_DIR}"
        CODEA_EXERCISE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

function(codea_gl_exercise target source)
    codea_console_exercise(${target} ${source})
    target_link_libraries(${target} PRIVATE glfw OpenGL::GL)
    if(APPLE)
        target_compile_definitions(${target} PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endfunction()

codea_console_exercise(chapter00 00_setup/chapter00.cpp)
codea_console_exercise(chapter01 01_math/chapter01.cpp)
codea_console_exercise(chapter02 01_math/chapter02.cpp)
codea_console_exercise(chapter03 01_math/chapter03.cpp)
codea_console_exercise(chapter04 02_cpu_rasterizer/chapter04.cpp)
codea_console_exercise(chapter05 02_cpu_rasterizer/chapter05.cpp)
codea_console_exercise(chapter06 02_cpu_rasterizer/chapter06.cpp)

codea_gl_exercise(chapter07 03_opengl_basics/chapter07.cpp)
codea_gl_exercise(chapter08 03_opengl_basics/chapter08.cpp)
codea_gl_exercise(chapter09 04_scene_camera_assets/chapter09.cpp)
codea_gl_exercise(chapter10 04_scene_camera_assets/chapter10.cpp)

codea_gl_exercise(chapter11 05_lighting_shadows/chapter11.cpp)
codea_gl_exercise(chapter12 05_lighting_shadows/chapter12.cpp)
codea_gl_exercise(chapter13 06_frame_renderer/chapter13.cpp)
codea_gl_exercise(chapter14 06_frame_renderer/chapter14.cpp)
codea_gl_exercise(chapter15 07_postprocess_ui/chapter15.cpp)
codea_gl_exercise(chapter16 07_postprocess_ui/chapter16.cpp)
codea_gl_exercise(chapter17 08_gameplay/chapter17.cpp)
codea_gl_exercise(chapter18 08_gameplay/chapter18.cpp)
if(APPLE)
    find_library(AUDIO_TOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(chapter18 PRIVATE
        ${AUDIO_TOOLBOX_FRAMEWORK}
        ${CORE_FOUNDATION_FRAMEWORK})
endif()
codea_console_exercise(chapter19 09_profiling_vulkan/chapter19.cpp)
codea_console_exercise(chapter20 09_profiling_vulkan/chapter20.cpp)
codea_gl_exercise(chapter21 10_capstone/chapter21.cpp)
if(APPLE)
    target_link_libraries(chapter21 PRIVATE
        ${AUDIO_TOOLBOX_FRAMEWORK}
        ${CORE_FOUNDATION_FRAMEWORK})
endif()

if(BUILD_TESTING)
    foreach(target IN ITEMS
            chapter00 chapter01 chapter02 chapter03 chapter04 chapter05 chapter06
            chapter20)
        add_test(NAME ${target}_smoke COMMAND $<TARGET_FILE:${target}>)
    endforeach()

    foreach(target IN ITEMS
            chapter07 chapter08 chapter09 chapter10 chapter11 chapter12 chapter13
            chapter14 chapter15 chapter16 chapter17 chapter18 chapter19 chapter21)
        add_test(NAME ${target}_smoke COMMAND $<TARGET_FILE:${target}> --check)
    endforeach()
endif()
