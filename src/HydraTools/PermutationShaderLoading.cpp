#include <HydraRuntime/Logger.h>
#include <HydraTools/FileCache.h>
#include <HydraTools/FileLocator.h>
#include <HydraTools/PermutationShaderLibrary.h>
#include <HydraTools/StringUtils.h>
#include <HydraTools/TextSectionizer.h>
#include <HydraTools/Tokenizer.h>

namespace Hydra::Tools
{
  const char* ShaderFileSection::SectionNames[ShaderFileSection::MAX_SECTIONS] = {
    "[VERTEX_SHADER]",
    "[HULL_SHADER]",
    "[DOMAIN_SHADER]",
    "[GEOMETRY_SHADER]",
    "[PIXEL_SHADER]",
    "[COMPUTE_SHADER]",
    "[USER_1]",
    "[USER_2]",
    "[USER_3]",
    "[USER_4]",
    "[USER_5]",
    "[USER_6]",
    "[USER_7]",
    "[USER_8]",
  };

  Runtime::Result PermutationShaderLibrary::ParseShaderImports(PermutationShader& shader, std::string_view imports) const
  {
    // TODO: use the Tokenizer instead

    while (!imports.empty())
    {
      std::string_view line = GetNextLine(imports);

      SkipWhitespace(line);
      TrimWhitespaceAtEnd(line);

      if (line.empty())
        continue;

      if (Accept(line, "//")) // line comments are allowed
        continue;

      if (Accept(line, "import"))
      {
        SkipWhitespace(line);

        if (auto moduleFile = m_fileLocator->FindFile(*m_fileCache, shader.m_normalizedPath, line); moduleFile.has_value())
        {
          shader.m_imports.push_back(moduleFile.value());
          continue;
        }
        else
        {
          Runtime::Log::Error(m_logger, "Could not locate file to import: %s", std::string(line).c_str());
          return Runtime::HYDRA_FAILURE;
        }
      }

      Runtime::Log::Error(m_logger, "Shader file starts with invalid statements: '%s'", std::string(line).c_str());
      return Runtime::HYDRA_FAILURE;
    }

    return Runtime::HYDRA_SUCCESS;
  }

  Runtime::Result PermutationShaderLibrary::LoadShaderImports(PermutationShader& shader)
  {
    for (const std::string& file : shader.m_imports)
    {
      if (LoadPermutationShader(file) == nullptr)
      {
        Runtime::Log::Error(m_logger, "Failed to import '%s'", file.c_str());
        return Runtime::HYDRA_FAILURE;
      }
    }

    return Runtime::HYDRA_SUCCESS;
  }

  Runtime::Result PermutationShaderLibrary::ParsePermutationConfiguration(std::map<std::string, std::string>& allowedPermutations, std::string_view permutations) const
  {
    Tokenizer tokenizer;
    tokenizer.Tokenize(permutations);

    enum class State
    {
      Idle,
      HasName,
      HasEqual,
      HasValue
    };

    State state = State::Idle;
    std::string variableName;

    for (const Token& token : tokenizer.GetResult())
    {
      if (token.m_type != Token::Type::Identifier && token.m_type != Token::Type::Integer && token.m_type != Token::Type::NewLine && token.m_type != Token::Type::NonIdentifier)
        continue;

      if (token.m_type == Token::Type::NewLine)
      {
        if (state == State::HasEqual)
        {
          Runtime::Log::Error(m_logger, "[PERMUTATIONS]: Missing assignment value: '%s = ?'", variableName.c_str());
          return Runtime::HYDRA_FAILURE;
        }

        if (state == State::HasName)
        {
          allowedPermutations[variableName] = {};
        }

        state = State::Idle;
        continue;
      }

      if (token.m_type == Token::Type::NonIdentifier)
      {
        if (token.m_token == "=" && state == State::HasName)
        {
          state = State::HasEqual;
          continue;
        }
      }
      else if (token.m_type == Token::Type::Identifier)
      {
        if (state == State::Idle)
        {
          variableName = token.m_token;
          state = State::HasName;
          continue;
        }

        if (state == State::HasEqual)
        {
          if (token.m_token == "*") // "A = *" is the same as giving it no value
            allowedPermutations[variableName] = {};
          else
            allowedPermutations[variableName] = token.m_token;

          state = State::HasValue;
          continue;
        }
      }

      Runtime::Log::Error(m_logger, "[PERMUTATIONS]: Malformed structure at token '%s'", std::string(token.m_token).c_str());
      return Runtime::HYDRA_FAILURE;
    }

    if (state == State::Idle || state == State::HasValue)
    {
      return Runtime::HYDRA_SUCCESS;
    }

    Runtime::Log::Error(m_logger, "[PERMUTATIONS]: Malformed structure at the end");
    return Runtime::HYDRA_FAILURE;
  }

  void PermutationShaderLibrary::ConfigureTextSectionizer(TextSectionizer& sectionizer) const
  {
    sectionizer.AddSection("");
    sectionizer.AddSection("[PERMUTATIONS]");
    sectionizer.AddSection("[ALL_SHADERS]");

    for (uint32_t sectionIdx = 0; sectionIdx < ShaderFileSection::MAX_SECTIONS; ++sectionIdx)
    {
      sectionizer.AddSection(ShaderFileSection::SectionNames[sectionIdx]);
    }
  }

  Runtime::Result PermutationShaderLibrary::ParseShaderFile(PermutationShader& shader, const std::string& content)
  {
    TextSectionizer sectionizer;

    // split the shader file into the expected sections
    ConfigureTextSectionizer(sectionizer);
    sectionizer.Process(content);

    // resolve shader imports
    {
      const std::string_view textImports = sectionizer.GetSectionContent(0);
      if (ParseShaderImports(shader, textImports).Failed())
      {
        Runtime::Log::Error(m_logger, "Resolving shader imports failed.");
        return Runtime::HYDRA_FAILURE;
      }

      if (LoadShaderImports(shader).Failed())
      {
        Runtime::Log::Error(m_logger, "Loading shader imports failed.");
        return Runtime::HYDRA_FAILURE;
      }
    }

    // determine which permutation variables (and fixed values) the user declared in the [PERMUTATIONS] section
    {
      const std::string_view textPermutations = sectionizer.GetSectionContent(1);
      if (ParsePermutationConfiguration(shader.m_allowedVariablePermutations, textPermutations).Failed())
      {
        Runtime::Log::Error(m_logger, "Invalid permutation variable configuration.");
        return Runtime::HYDRA_FAILURE;
      }
    }

    // read all of the shader + user sections
    // replace #include statements
    // figure out which permutation variables are used
    {
      // load the common shader text
      const std::string_view textCommon = sectionizer.GetSectionContent(2);

      std::string fullSection;
      std::set<std::string> alreadyIncluded;

      for (uint32_t sectionIdx = 0; sectionIdx < ShaderFileSection::MAX_SECTIONS; ++sectionIdx)
      {
        const std::string_view textShader = sectionizer.GetSectionContent(3 + sectionIdx); // +3 because we have three additional sections before this

        if (sectionIdx < ShaderFileSection::User1)
        {
          // the known shader code sections get the common shader source prepended
          fullSection = textCommon;
          fullSection += textShader;
        }
        else
        {
          // the user sections stay as they are
          fullSection = textShader;
        }

        alreadyIncluded.clear();
        fullSection = ReplaceHashIncludes(shader.m_normalizedPath, fullSection, alreadyIncluded, *m_fileLocator, *m_fileCache, m_logger);
        shader.m_shaderSection[sectionIdx].SetText(std::move(fullSection));

        // keep track of all the files that were #include'd
        for (const auto& file : alreadyIncluded)
        {
          shader.m_referencedFiles.insert(file);
        }

        if (shader.m_shaderSection[sectionIdx].DetermineUsedPermutationVariables(shader.m_usedPermutationVariables, m_logger).Failed())
        {
          Runtime::Log::Error(m_logger, "The shader section '%s' has an erroneous permutation condition.", ShaderFileSection::SectionNames[sectionIdx]);
          return Runtime::HYDRA_FAILURE;
        }
      }
    }

    return Runtime::HYDRA_SUCCESS;
  }

  const Hydra::Tools::PermutationShader* PermutationShaderLibrary::LoadPermutationShader(std::string_view path)
  {
    if (m_fileCache == nullptr || m_fileLocator == nullptr)
    {
      Runtime::Log::Error(m_logger, "PermutationShaderLibrary: FileCache and FileLocator are not set up.");
      return nullptr;
    }

    std::string finalPath;
    finalPath = path;

    m_fileCache->NormalizeFilePath(finalPath);
    std::optional<std::string> filePath = m_fileLocator->FindFile(*m_fileCache, "", finalPath);

    if (!filePath.has_value())
    {
      // file not found -> not an error here
      Runtime::Log::Info(m_logger, "LoadPermutationShader: File '%s' does not exist.", finalPath.c_str());
      return nullptr;
    }

    finalPath = filePath.value();

    std::scoped_lock<std::recursive_mutex> lk(m_mutex);

    std::map<std::string, PermutationShader>::const_iterator it = m_loadedShaders.find(finalPath);

    if (it != m_loadedShaders.end())
    {
      Runtime::Log::Info(m_logger, "Permutation shader '%s' already loaded.", finalPath.c_str());
      return &it->second;
    }

    {
      Runtime::Log::Info(m_logger, "Loading permutation shader '%s'", finalPath.c_str());

      PermutationShader& shader = m_loadedShaders[finalPath];
      shader.m_normalizedPath = finalPath;

      if (ParseShaderFile(shader, m_fileCache->GetFileContent(finalPath)).Failed())
      {
        Runtime::Log::Error(m_logger, "Loading permutation shader '%s' failed.", finalPath.c_str());

        m_loadedShaders.erase(finalPath);
        return nullptr;
      }

      if (ValidateShader(shader).Failed())
      {
        Runtime::Log::Error(m_logger, "Validating permutation shader '%s' failed.", finalPath.c_str());

        m_loadedShaders.erase(finalPath);
        return nullptr;
      }

      Runtime::Log::Info(m_logger, "Successfully loaded permutation shader '%s'", finalPath.c_str());
      return &shader;
    }
  }

  Runtime::Result PermutationShaderLibrary::ValidateShader(PermutationShader& shader) const
  {
    // since imports are loaded the same way, they are already validated, and we don't need to validate the full chain here

    // TODO: check that imports have no cycles
    // TODO: check that fixed values are in range / type
    // TODO: check that used enum values are valid
    // TODO: check that all used vars are registered

    Runtime::Result res = Runtime::HYDRA_SUCCESS;

    std::set<std::string> usedVariables;
    GetAllUsedPermutationVariables(shader, usedVariables);

    // check that all used variables are declared in the [PERMUTATIONS] section
    {
      std::map<std::string, std::string> allowedValues;
      GetAllowedVariablePermutations(shader, allowedValues);

      for (const std::string& usedVar : usedVariables)
      {
        if (usedVar.find("::") != std::string::npos)
        {
          // TODO: validate enum values better
          continue;
        }

        if (allowedValues.find(usedVar) == allowedValues.end())
        {
          res = Runtime::HYDRA_FAILURE;

          Runtime::Log::Error(m_logger, "Shader uses permutation variable '%s' that isn't declared in its [PERMUTATIONS] section.", usedVar.c_str());
        }
      }

      return res;
    }

    // check that declared variables only restrict their values, compared to imported shaders
    {
      std::map<std::string, std::string> allowedValues;
      GetAllowedVariablePermutations(shader, allowedValues);

      for (const std::string& dependency : shader.m_imports)
      {
        const PermutationShader* subShader = GetLoadedPermutationShader(dependency);
        assert(subShader != nullptr);

        GetAllowedVariablePermutations(*subShader, allowedValues);
      }

      for (const auto iterNew : shader.m_allowedVariablePermutations)
      {
        auto iterOld = allowedValues.find(iterNew.first);

        if (iterOld == allowedValues.end())
        {
          // variable is yet unknown -> insert it
          allowedValues[iterNew.first] = iterNew.second;
        }
        else
        {
          // variable already known -> make sure it's either the same or more restrictive

          if (iterOld->second.empty() || iterOld->second == iterNew.second)
          {
            // existing value is identical to new one -> fine
            // existing value allows all permutations -> fine, take the new value (may be more restrictive or not)
            allowedValues[iterNew.first] = iterNew.second;
          }
          else
          {
            res = Runtime::HYDRA_FAILURE;

            Runtime::Log::Error(m_logger, "Allowed values for '%s' are conflicting with imported shaders: '%s' != '%s'", iterNew.first.c_str(), iterOld->second.c_str(), iterNew.second.c_str());
          }
        }
      }
    }

    return res;
  }

} // namespace Hydra::Tools
