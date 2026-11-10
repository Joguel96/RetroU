cmake_minimum_required(VERSION 3.20)
project(retrou_card_prototype LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2/SDL2-2.30.10")
set(SDL2_TTF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2_ttf/SDL2_ttf-2.22.0")
set(SDL2_IMAGE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2_image/SDL2_image-2.8.2")

add_executable(card_demo main.cpp)

target_include_directories(card_demo PRIVATE
    "${SDL2_ROOT}/include"
    "${SDL2_TTF_ROOT}/include"
    "${SDL2_IMAGE_ROOT}/include"
)

target_link_directories(card_demo PRIVATE
    "${SDL2_ROOT}/lib/x64"
    "${SDL2_TTF_ROOT}/lib/x64"
    "${SDL2_IMAGE_ROOT}/lib/x64"
)

target_link_libraries(card_demo PRIVATE
    SDL2.lib
    SDL2_ttf.lib
    SDL2_image.lib
)

add_custom_command(TARGET card_demo POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SDL2_ROOT}/lib/x64/SDL2.dll" "$<TARGET_FILE_DIR:card_demo>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SDL2_TTF_ROOT}/lib/x64/SDL2_ttf.dll" "$<TARGET_FILE_DIR:card_demo>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${SDL2_IMAGE_ROOT}/lib/x64/SDL2_image.dll" "$<TARGET_FILE_DIR:card_demo>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:card_demo>/assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../content" "$<TARGET_FILE_DIR:card_demo>/content"
    COMMENT "Copying SDL2 DLLs, font and prototype assets"
)
