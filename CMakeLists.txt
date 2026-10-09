cmake_minimum_required(VERSION 3.20)
project(ais_simulator LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Point IXWebSocket's find_package(MbedTLS) at our manually-built copy ---
# These are the exact variable names FindPackageHandleStandardArgs was
# reporting as missing earlier — now they point at real files instead of
# guessed target names, since we built and vendored mbedTLS ourselves.
set(MBEDTLS_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/external/mbedtls/include" CACHE PATH "" FORCE)
set(MBEDTLS_LIBRARY "${CMAKE_SOURCE_DIR}/external/mbedtls/lib/mbedtls.lib" CACHE FILEPATH "" FORCE)
set(MBEDX509_LIBRARY "${CMAKE_SOURCE_DIR}/external/mbedtls/lib/mbedx509.lib" CACHE FILEPATH "" FORCE)
set(MBEDCRYPTO_LIBRARY "${CMAKE_SOURCE_DIR}/external/mbedtls/lib/mbedcrypto.lib" CACHE FILEPATH "" FORCE)


#--- FFTW3
set(FFTW3_DIR "${CMAKE_SOURCE_DIR}/external/fftw3")

# --- IXWebSocket ---
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG        v11.4.5
)
set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_MBED_TLS ON CACHE BOOL "" FORCE)

# --- nlohmann/json ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)

#GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

#GLM
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
)

#stb
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
)

#assimp
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)



FetchContent_MakeAvailable(ixwebsocket nlohmann_json glfw glm stb assimp)

add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)



file(GLOB_RECURSE AIS_SIM_SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_executable(ais_simulator ${AIS_SIM_SOURCES})

add_custom_command(TARGET ais_simulator POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/.env $<TARGET_FILE_DIR:ais_simulator>/.env
)

add_custom_command(TARGET ais_simulator POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/shaders $<TARGET_FILE_DIR:ais_simulator>/shaders
)

add_custom_command(TARGET ais_simulator POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/textures $<TARGET_FILE_DIR:ais_simulator>/textures
)

add_custom_command(TARGET ais_simulator POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/models $<TARGET_FILE_DIR:ais_simulator>/models
)

target_include_directories(ais_simulator PRIVATE include)
target_include_directories(ais_simulator PRIVATE ${stb_SOURCE_DIR})
target_link_directories(ais_simulator PRIVATE external/mbedtls/lib)

target_include_directories(ais_simulator PRIVATE ${FFTW3_DIR})
target_link_libraries(ais_simulator PRIVATE ${FFTW3_DIR}/libfftw3f-3.lib)

target_link_libraries(ais_simulator PRIVATE assimp)

add_custom_command(TARGET ais_simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${FFTW3_DIR}/libfftw3f-3.dll
    $<TARGET_FILE_DIR:ais_simulator>
)


target_link_libraries(ais_simulator PRIVATE
  ixwebsocket
  nlohmann_json::nlohmann_json
  mbedtls
  mbedx509
  mbedcrypto
  glfw
  glad
  opengl32
  glm::glm
  assimp
)