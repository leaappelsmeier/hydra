#include <HydraRuntime/Logger.h>
#include <HydraRuntime/PermutationManager.h>
#include <HydraTools/PermutationShaderLibrary.h>
#include <assert.h>

namespace Hydra::Tools
{

  void PermutationShaderLibrary::GetAllUsedPermutationVariables(const PermutationShader& shader, std::set<std::string>& allUsedVariables) const
  {
    for (const std::string& importShader : shader.m_imports)
    {
      const PermutationShader* subShader = GetLoadedPermutationShader(importShader);
      assert(subShader != nullptr);

      GetAllUsedPermutationVariables(*subShader, allUsedVariables);
    }

    for (const std::string& var : shader.m_usedPermutationVariables)
    {
      allUsedVariables.insert(var);
    }
  }

  void PermutationShaderLibrary::GetAllReferencedFiles(const PermutationShader& shader, std::set<std::string>& allReferencedFiles) const
  {
    // own path
    allReferencedFiles.insert(shader.m_normalizedPath);

    // all #include'd files
    for (const std::string& file : shader.m_referencedFiles)
    {
      allReferencedFiles.insert(file);
    }

    // all imports
    for (const std::string& dependency : shader.m_imports)
    {
      const PermutationShader* subShader = GetLoadedPermutationShader(dependency);
      assert(subShader != nullptr);

      GetAllReferencedFiles(*subShader, allReferencedFiles);
    }
  }

  void PermutationShaderLibrary::GetAllowedVariablePermutations(const PermutationShader& shader, std::map<std::string, std::string>& allowedVariableValues) const
  {
    // note that this function does NOT recursively pull in the declarations from imported shaders
    // see this function's documentation for more details

    allowedVariableValues = shader.m_allowedVariablePermutations;
  }

  std::optional<std::string> PermutationShaderLibrary::GeneratePermutedShaderCode(const PermutationShader& shader, ShaderFileSection::Enum stage, const PermutationVariableValues& permutationVariables) const
  {
    std::string result;

    for (const std::string& importedShader : shader.m_imports)
    {
      const PermutationShader* subShader = GetLoadedPermutationShader(importedShader);
      assert(subShader != nullptr);

      if (auto code = GeneratePermutedShaderCode(*subShader, stage, permutationVariables); code.has_value())
      {
        result += code.value();
      }
      else
      {
        Runtime::Log::Error(m_logger, "Failed to generate text permutation for import '%s'", subShader->m_normalizedPath.c_str());
        return {};
      }
    }

    if (auto code = shader.m_shaderSection[stage].GenerateTextPermutation(permutationVariables, m_logger))
    {
      result += code.value();
    }
    else
    {
      Runtime::Log::Error(m_logger, "Failed to generate text permutation for '%s'", shader.m_normalizedPath.c_str());
      return {};
    }

    return result;
  }

  Hydra::Runtime::PermutationVariableSet PermutationShaderLibrary::CreatePermutationVariableSet(const PermutationShader& shader, const Runtime::PermutationManager& permutationManager)
  {
    std::map<std::string, std::string> allowedVarValues;
    GetAllowedVariablePermutations(shader, allowedVarValues);

    Runtime::PermutationVariableSet permVarSet;

    for (const auto iter : allowedVarValues)
    {
      if (!iter.second.empty())
      {
        // skip all variables that have fixed values -> they are not needed for the permutation selection
        continue;
      }

      if (const Runtime::PermutationVariableEntry* varEntry = permutationManager.GetVariable(iter.first.c_str()))
      {
        permVarSet.AddVariable(*varEntry);
      }
      else
      {
        Runtime::Log::Error(m_logger, "CreatePermutationVariableSet failed: Variable '%s' does not exist. Shader = '%s'", iter.first.c_str(), shader.m_normalizedPath.c_str());

        return Runtime::PermutationVariableSet();
      }
    }

    return permVarSet;
  }

  Runtime::Result PermutationShaderLibrary::SetupVariableValuesForPermutationSelection(PermutationVariableValues& variables, const PermutationShader& shader, const Runtime::PermutationManager& manager, const Runtime::PermutationVariableSelection& selection)
  {
    variables.clear();

    std::map<std::string, std::string> allowedValues;
    GetAllowedVariablePermutations(shader, allowedValues);

    if (SetupVariableValuesWithSelectionValues(variables, selection).Failed())
    {
      Runtime::Log::Error(m_logger, "Failed to setup variables from selection.");
      return Runtime::HYDRA_FAILURE;
    }

    if (SetupVariableValuesWithNeededEnumValues(variables, shader, manager, allowedValues).Failed())
    {
      Runtime::Log::Error(m_logger, "Failed to setup required enum values.");
      return Runtime::HYDRA_FAILURE;
    }

    if (SetupVariableValuesWithFixedValues(variables, shader, manager, allowedValues).Failed())
    {
      Runtime::Log::Error(m_logger, "Failed to setup fixed permutation variable values.");
      return Runtime::HYDRA_FAILURE;
    }

    return Runtime::HYDRA_SUCCESS;
  }

  Runtime::Result PermutationShaderLibrary::SetupVariableValuesWithNeededEnumValues(PermutationVariableValues& variables, const PermutationShader& shader, const Runtime::PermutationManager& manager, const std::map<std::string, std::string>& allowedValues)
  {
    std::string identifier;

    for (auto iter : allowedValues)
    {
      const std::string& varName = iter.first;

      const Runtime::PermutationVariableEntry* variable = manager.GetVariable(varName.c_str());

      if (variable == nullptr)
      {
        Runtime::Log::Error(m_logger, "Permutation variable '%s' does not exist.", varName.c_str());
        return Runtime::HYDRA_FAILURE;
      }

      if (variable->m_type != Runtime::PermutationVariableEntry::Type::Enum)
        continue;

      for (const auto& value : variable->m_allowedValues)
      {
        identifier = variable->m_name + "::" + value.first;

        variables[identifier] = value.second;
      }
    }

    return Runtime::HYDRA_SUCCESS;
  }

  Runtime::Result PermutationShaderLibrary::SetupVariableValuesWithSelectionValues(PermutationVariableValues& variables, const Runtime::PermutationVariableSelection& selection) const
  {
    selection.Iterate([&](const Runtime::PermutationVariableEntry& var, int intValue, const char* value)
      { variables[var.m_name] = intValue; });

    return Runtime::HYDRA_SUCCESS;
  }

  Runtime::Result PermutationShaderLibrary::SetupVariableValuesWithFixedValues(PermutationVariableValues& variables, const PermutationShader& shader, const Runtime::PermutationManager& manager, const std::map<std::string, std::string>& allowedValues)
  {
    for (const auto iter : allowedValues)
    {
      if (iter.second.empty())
        continue;

      const Runtime::PermutationVariableEntry* permVar = manager.GetVariable(iter.first.c_str());

      if (permVar == nullptr)
      {
        Runtime::Log::Error(m_logger, "Permutation variable '%s' does not exist.", iter.first.c_str());
        return Runtime::HYDRA_FAILURE;
      }

      if (permVar->m_type == Runtime::PermutationVariableEntry::Type::Bool)
      {
        if (iter.second == "true")
          variables[iter.first] = 1;
        else
          variables[iter.first] = 0;
      }
      else if (permVar->m_type == Runtime::PermutationVariableEntry::Type::Int)
      {
        variables[iter.first] = atoi(iter.second.c_str());
      }
      else if (permVar->m_type == Runtime::PermutationVariableEntry::Type::Enum)
      {
        for (const auto& value : permVar->m_allowedValues)
        {
          if (value.first == iter.second)
          {
            variables[iter.first] = value.second;
            break;
          }
        }
      }
    }

    return Runtime::HYDRA_SUCCESS;
  }
} // namespace Hydra::Tools
