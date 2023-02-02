cmake_minimum_required(VERSION 3.20)

project(Hydra)
set(CMAKE_CXX_STANDARD 20)

set(RUNTIME_FILES 
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraRuntime/BitSet.h"
  	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraRuntime/BitSet.inl"
  	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraRuntime/Core.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraRuntime/HydraRuntime.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/HydraRuntime/Logger.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/HydraRuntime/PermutationManager.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/HydraRuntime/PermutationSets.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraRuntime/Result.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HydraRuntime/BitSet.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HydraRuntime/Core.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HydraRuntime/Logger.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraRuntime/PermutationManager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HydraRuntime/PermutationSets.cpp"
)

add_library(HydraRuntime ${RUNTIME_FILES})

set(TOOLS_FILES 
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraTools/Evaluator.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraTools/PermutationShader.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraTools/PermutationShaderLibrary.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraTools/FileCache.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraTools/FileLocator.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraTools/Tokenizer.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraTools/PermutableText.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraTools/PermutationVariableLoader.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/HydraTools/TextSectionizer.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/Evaluator.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/PermutationShaderLoading.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/PermutationShaderUsage.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/PermutationShaderLibrary.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/FileCache.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/FileLocator.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/Tokenizer.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/PermutableText.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/PermutationVariableLoader.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/StringUtils.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/StringUtils.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/TextSectionizer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/HydraTools/Thirdparty/json.hpp"
)

add_library(HydraTools ${TOOLS_FILES})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${RUNTIME_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${TOOLS_FILES})

target_compile_definitions(HydraRuntime PUBLIC "HYDRA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

target_include_directories(HydraRuntime PUBLIC "include")
target_include_directories(HydraRuntime PRIVATE "src")

target_link_libraries(HydraTools PUBLIC HydraRuntime)
target_include_directories(HydraTools PRIVATE "src")

add_executable(HydraSample
	"sample/HydraSample.cpp"
)

target_link_libraries(HydraSample PRIVATE HydraTools)

add_executable(HydraUnitTests
    "unittests/UnitTests.cpp"
    "unittests/RuntimeTest.cpp"
    "unittests/RuntimeTest.h"
    "unittests/ToolsTest.cpp"
    "unittests/ToolsTest.h"
    "unittests/thirdparty/munit.c"
)

target_link_libraries(HydraUnitTests PRIVATE HydraTools)
