#include <HydraRuntime/HydraRuntime.h>
#include <HydraTools/Evaluator.h>
#include <HydraTools/FileCache.h>
#include <HydraTools/FileLocator.h>
#include <HydraTools/PermutationShader.h>
#include <HydraTools/PermutationShaderLibrary.h>
#include <HydraTools/PermutationVariableLoader.h>

using namespace Hydra;
using namespace Hydra::Runtime;
using namespace Hydra::Tools;

struct SampleLogger : public ILoggingInterface
{
  void LogInfo(const char* message) override
  {
    fprintf(stdout, "%s\n", message);
  }

  void LogWarning(const char* message) override
  {
    fprintf(stdout, "Warning: %s\n", message);
  }

  void LogError(const char* message) override
  {
    fprintf(stderr, "Error: %s\n", message);
  }
};

class DemoRenderer
{
public:
  DemoRenderer()
  {
    {
      // this demo doesn't actually render anything
      // for demonstrative purposes, we use a custom section in the ".Hydra" file
      // but in practice you would use the vertex and pixel shader sections instead
      Tools::ShaderFileSection::SectionNames[Tools::ShaderFileSection::User1] = "[USERSECTION_DESCRIPTION]";

      m_fileLocator.AddIncludeDirectory(HYDRA_DIR "/sample"); // absolute path where to find our shaders

      // we use the Tools infrastructure for shader loading and text permutation generation
      // though this is not mandatory, you could use something custom as well
      m_shaderLibrary.SetLogger(&m_logger);
      m_shaderLibrary.SetFileCache(&m_fileCache);
      m_shaderLibrary.SetFileLocator(&m_fileLocator);
    }

    // set up all the permutation variables that we need
    {
      // part 1: load permutation variables from json file
      Tools::PermutationVariableLoader permVarLoader(&m_logger);
      permVarLoader.SetFileCache(&m_fileCache);
      permVarLoader.SetFileLocator(&m_fileLocator);
      permVarLoader.RegisterVariablesFromJsonFile(m_permutationManager, "data/PermutationVariables.json").IgnoreResult();

      // part 2: retrieve permutation variables for use at runtime
      m_permVarUseFog = m_permutationManager.GetVariable("USE_FOG");
      m_permVarUseNormalMap = m_permutationManager.GetVariable("USE_NORMALMAP");
      m_permVarLightingMode = m_permutationManager.GetVariable("LIGHTING_MODE");

      // part 3: register additional permutation variables as required
      m_permVarUseMotionBlur = m_permutationManager.RegisterVariable("USE_MOTIONBLUR", true);

      std::vector<std::pair<std::string, int>> renderModeValues;
      renderModeValues.push_back({"DEPTHONLY", 0});
      renderModeValues.push_back({"WIREFRAME", 11});
      renderModeValues.push_back({"REGULAR", 42});
      m_permVarRenderMode = m_permutationManager.RegisterVariable("RENDER_MODE", renderModeValues, 42);

      std::vector<std::pair<std::string, int>> reflectionValues;
      reflectionValues.push_back({"NONE", 1});
      reflectionValues.push_back({"SCREENSPACE", 2});
      reflectionValues.push_back({"RAYTRACED", 3});
      m_permVarReflections = m_permutationManager.RegisterVariable("REFLECTIONS", reflectionValues, 2);
    }
  }

  size_t LoadPermutationShader(std::string_view path)
  {
    m_shaders.push_back(DemoShader());
    DemoShader& shader = m_shaders.back();

    // in this demo we will load the full shader and all data about it right away
    // in a proper engine one would either pre-compile shaders or at least cache compiled permutations
    // and therefore only do this, when a shader permutation actually has to be compiled
    shader.m_permutationShader = m_shaderLibrary.LoadPermutationShader(path);

    if (shader.m_permutationShader != nullptr)
    {
      // if the shader was loaded without errors

      shader.m_permutationVariableSet = m_shaderLibrary.CreatePermutationVariableSet(*shader.m_permutationShader, m_permutationManager);
    }

    return m_shaders.size() - 1;
  }

  void BindShader(size_t shaderId)
  {
    m_activeShaderId = shaderId;
  }

  void MakeDrawcall()
  {
    DemoShader& shader = m_shaders[m_activeShaderId];

    if (shader.m_permutationShader == nullptr)
    {
      printf("Skipping drawcall, because this shader is broken.\n");
      return;
    }

    Runtime::PermutationVariableState finalState;

    if (PermutationVariableState::MergeBontoA(m_GlobalVariableState, m_ActiveMaterialVariableState, shader.m_permutationVariableSet, finalState).Failed())
    {
      printf("Skipping drawcall, because merging permutation states failed.\n");
      return;
    }

    PermutationVariableSelection permutationSelection;
    if (m_permutationManager.FinalizeState(finalState, shader.m_permutationVariableSet, permutationSelection).Failed())
    {
      printf("Skipping drawcall, because the permutation selection failed.\n");
      return;
    }

    const uint32_t selectionHash = permutationSelection.Hash();

    auto iter = shader.m_shaderPermutations.find(selectionHash);

    if (iter == shader.m_shaderPermutations.end())
    {
      printf("Shader permutation %u doesn't exist yet, generating...\n", selectionHash);

      PermutationVariableValues values;
      if (m_shaderLibrary.SetupVariableValuesForPermutationSelection(values, *shader.m_permutationShader, m_permutationManager, permutationSelection).Failed())
      {
        printf("Skipping drawcall, because the shader uses unknown permutation variables.\n");
        return;
      }

      std::optional<std::string> permutationSrc = m_shaderLibrary.GeneratePermutedShaderCode(*shader.m_permutationShader, ShaderFileSection::User1, values);

      if (!permutationSrc.has_value())
      {
        printf("Skipping drawcall, because the shader permutation generation failed.\n");
        return;
      }

      shader.m_shaderPermutations[selectionHash] = permutationSrc.value();
      iter = shader.m_shaderPermutations.find(selectionHash);
    }

    printf("\nDoing drawcall:\n%s\n", iter->second.c_str());
  }

  const PermutationVariableEntry* m_permVarUseFog = nullptr;
  const PermutationVariableEntry* m_permVarUseNormalMap = nullptr;
  const PermutationVariableEntry* m_permVarUseMotionBlur = nullptr;
  const PermutationVariableEntry* m_permVarLightingMode = nullptr;
  const PermutationVariableEntry* m_permVarRenderMode = nullptr;
  const PermutationVariableEntry* m_permVarReflections = nullptr;

  Runtime::PermutationVariableState m_GlobalVariableState;
  Runtime::PermutationVariableState m_ActiveMaterialVariableState;

private:
  struct DemoShader
  {
    const Tools::PermutationShader* m_permutationShader = nullptr;
    Runtime::PermutationVariableSet m_permutationVariableSet;
    std::map<uint32_t, std::string> m_shaderPermutations;
  };

  SampleLogger m_logger;
  Tools::FileCacheStdFileSystem m_fileCache;
  Tools::FileLocatorStd m_fileLocator;
  Tools::PermutationShaderLibrary m_shaderLibrary;
  std::deque<DemoShader> m_shaders;
  size_t m_activeShaderId = 0;
  Runtime::PermutationManager m_permutationManager;
};

void Demo()
{
  DemoRenderer renderer;

  const size_t shaderExample1 = renderer.LoadPermutationShader("data/Example.hydra");
  renderer.BindShader(shaderExample1);

  {
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarLightingMode, 2).IgnoreResult();
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarRenderMode, 0).IgnoreResult();
    renderer.MakeDrawcall();
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarRenderMode, 42).IgnoreResult();
  }

  {
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarUseFog, true).IgnoreResult();
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarUseNormalMap, true).IgnoreResult();
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarUseMotionBlur, false).IgnoreResult();

    renderer.MakeDrawcall();
  }

  {
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarUseFog, false).IgnoreResult();
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarUseNormalMap, false).IgnoreResult();
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarUseMotionBlur, true).IgnoreResult();
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarLightingMode, 1).IgnoreResult();
    renderer.m_GlobalVariableState.SetVariable(*renderer.m_permVarReflections, 0).IgnoreResult();

    renderer.m_ActiveMaterialVariableState.SetVariable(*renderer.m_permVarReflections, 2).IgnoreResult();

    renderer.MakeDrawcall();
  }
}

int main()
{
  Demo();

  return 0;
}
