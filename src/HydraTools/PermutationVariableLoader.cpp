#include <HydraTools/PermutationVariableLoader.h>

#include <HydraRuntime/Logger.h>
#include <HydraRuntime/PermutationManager.h>
#include <HydraTools/FileCache.h>
#include <HydraTools/FileLocator.h>

#include "thirdparty/json.hpp"
#include <algorithm>
#include <iostream>
#include <ranges>

using namespace Hydra::Runtime;

namespace
{
  using json = nlohmann::json;

  const char* GetTypeName(json::value_t type)
  {
    switch (type)
    {
      case json::value_t::null:
        return "null";
      case json::value_t::object:
        return "object";
      case json::value_t::array:
        return "array";
      case json::value_t::string:
        return "string";
      case json::value_t::boolean:
        return "boolean";
      case json::value_t::number_integer:
        return "number_integer";
      case json::value_t::number_unsigned:
        return "number_unsigned";
      case json::value_t::number_float:
        return "number_float";
      case json::value_t::binary:
        return "binary";
      default:
        return "<invalid>";
    }
  }

  std::optional<json::value_t> EvaluatePermutationVarType(
    const std::string& key, const json& value, Hydra::Runtime::ILoggingInterface* logger)
  {
    std::string typeString = value.value("Type", "<invalid>");
    json::value_t expectedTypeForDefault = json::value_t::null;
    json defaultValue;
    if (typeString == "bool")
    {
      return json::value_t::boolean;
    }
    else if (typeString == "int")
    {
      return json::value_t::number_integer;
    }
    else if (typeString == "enum")
    {
      return json::value_t::array;
    }
    else if (typeString != "<invalid>")
    {
      Log::Error(logger, "RegisterVariablesFromJsonFile: Invalid type '%s' for key '%s'", typeString.c_str(), key.c_str());
    }
    else
    {
      Log::Error(logger, "RegisterVariablesFromJsonFile: Unable to find type information for key '%s'", key.c_str());
    }

    return std::nullopt;
  }

  Result RegisterBoolPermutationVar(
    PermutationManager& permMgr, const std::string& key, const json& value, Hydra::Runtime::ILoggingInterface* logger)
  {
    // Check for default value
    std::optional<bool> defaultValue;
    if (auto it = value.find("Default"); it != value.end())
    {
      if (it.value().is_boolean())
      {
        defaultValue = it.value();
      }
      else
      {
        Log::Error(
          logger, "RegisterVariablesFromJsonFile: Invalid type '%s' as default value for bool permutation variable '%s'", GetTypeName(it.value().type()), key.c_str());
        return HYDRA_FAILURE;
      }
    }

    return permMgr.RegisterVariable(key.c_str(), defaultValue) != nullptr ? HYDRA_SUCCESS : HYDRA_FAILURE;
  }

  Result RegisterIntPermutationVar(PermutationManager& permMgr, const std::string& key, const json& value, ILoggingInterface* logger)
  {
    // Retrieve allowed values
    std::vector<int> allowedValues;
    if (auto it = value.find("Values"); it != value.end())
    {
      if (it.value().is_array())
      {
        for (const auto& item : it.value())
        {
          if (item.is_number_unsigned() || item.is_number_integer())
          {
            allowedValues.push_back(item);
          }
          else
          {
            Log::Error(
              logger, "RegisterVariablesFromJsonFile: Invalid item of type '%s' in values array for int permutation variable '%s'", GetTypeName(item.type()), key.c_str());
            return HYDRA_FAILURE;
          }
        }
        std::vector<int> values = it.value();
        allowedValues.swap(values);
      }
      else
      {
        Log::Error(
          logger, "RegisterVariablesFromJsonFile: Invalid type '%s' of values array for int permutation variable '%s'", GetTypeName(it.value().type()), key.c_str());
        return HYDRA_FAILURE;
      }
    }

    // Check for default value
    std::optional<int> defaultValue;
    if (auto it = value.find("Default"); it != value.end())
    {
      if (it.value().is_number_unsigned() || it.value().is_number_integer())
      {
        defaultValue = it.value();
      }
      else
      {
        Log::Error(
          logger, "RegisterVariablesFromJsonFile: Invalid type '%s' as default value for int permutation variable '%s'", GetTypeName(it.value().type()), key.c_str());
        return HYDRA_FAILURE;
      }
    }

    return permMgr.RegisterVariable(key.c_str(), allowedValues, defaultValue) != nullptr ? HYDRA_SUCCESS : HYDRA_FAILURE;
  }

  Result AddSingularKeyValuePair(
    const std::string& key, const json& element, std::vector<std::pair<std::string, int>>& valueList, ILoggingInterface* logger)
  {
    if (element.is_object() && element.size() == 1)
    {
      const auto& entry = element.items().begin();
      if (entry.value().is_number_unsigned() || entry.value().is_number_integer())
      {
        valueList.push_back({entry.key(), entry.value()});
      }
    }
    else
    {
      Log::Error(logger, "RegisterVariablesFromJsonFile: Invalid entry in values array for int permutation variable '%s'", key.c_str());
      return HYDRA_FAILURE;
    }

    return HYDRA_SUCCESS;
  }

  Result RegisterEnumPermutationVar(PermutationManager& permMgr, const std::string& key, const json& value, ILoggingInterface* logger)
  {
    // Retrieve allowed values
    std::vector<std::pair<std::string, int>> allowedValues;
    if (auto it = value.find("Values"); it != value.end())
    {
      if (it.value().is_array())
      {
        for (const auto& item : it.value())
        {
          if (AddSingularKeyValuePair(key, item, allowedValues, logger).Failed())
          {
            return HYDRA_FAILURE;
          }
        }
      }
      else
      {
        Log::Error(
          logger, "RegisterVariablesFromJsonFile: Invalid type '%s' of values array for int permutation variable '%s'", GetTypeName(it.value().type()), key.c_str());
        return HYDRA_FAILURE;
      }
    }

    // Check for default value
    std::optional<int> defaultValue;
    if (auto itDefault = value.find("Default"); itDefault != value.end())
    {
      if (itDefault.value().is_string())
      {
        auto defaultValueString = itDefault.value().get<std::string>();

        auto is_same = [&defaultValueString](const std::pair<std::string, int>& val)
        {
          return val.first == defaultValueString;
        };

        auto itEntry = std::find_if(allowedValues.cbegin(), allowedValues.cend(), is_same);
        if (itEntry != allowedValues.cend())
        {
          defaultValue = itEntry->second;
        }
        else
        {
          Log::Error(logger,
            "RegisterVariablesFromJsonFile: Unable to find entry for '%s' in values array for enum permutation variable '%s'",
            std::string(itDefault.value()).c_str(),
            key.c_str());
          return HYDRA_FAILURE;
        }
      }
      else
      {
        Log::Error(logger,
          "RegisterVariablesFromJsonFile: Invalid type '%s' as default value for enum permutation variable '%s' - expected '%s'",
          GetTypeName(itDefault.value().type()),
          key.c_str(),
          GetTypeName(json::value_t::string));
        return HYDRA_FAILURE;
      }
    }

    return permMgr.RegisterVariable(key.c_str(), allowedValues, defaultValue) != nullptr ? HYDRA_SUCCESS : HYDRA_FAILURE;
  }
} // namespace

namespace Hydra::Tools
{

  PermutationVariableLoader::PermutationVariableLoader(Runtime::ILoggingInterface* logger)
    : m_logger(logger)
  {
  }

  PermutationVariableLoader::~PermutationVariableLoader() = default;

  void PermutationVariableLoader::SetFileCache(FileCache* cache)
  {
    m_fileCache = cache;
  }

  void PermutationVariableLoader::SetFileLocator(FileLocator* locator)
  {
    m_fileLocator = locator;
  }

  Result PermutationVariableLoader::RegisterVariablesFromJsonFile(PermutationManager& permMgr, std::string_view path, bool ignoreComments)
  {
    if (m_fileCache == nullptr || m_fileLocator == nullptr)
    {
      Runtime::Log::Error(m_logger, "PermutationVariableLoader: FileCache and FileLocator are not set up.");
      return HYDRA_FAILURE;
    }

    std::string finalPath(path);
    m_fileCache->NormalizeFilePath(finalPath);
    std::optional<std::string> filePath = m_fileLocator->FindFile(*m_fileCache, "", finalPath);

    if (!filePath.has_value())
    {
      Log::Error(m_logger, "RegisterVariablesFromJsonFile: Json file '%s' could not be found.", finalPath.c_str());
      return HYDRA_FAILURE;
    }

    std::string contentString = m_fileCache->GetFileContent(*filePath);

    using json = nlohmann::json;
    json content = json::parse(contentString, nullptr, false, ignoreComments);
    if (content.is_discarded())
    {
      Log::Error(m_logger, "RegisterVariablesFromJsonFile: Error while parsing json file '%s'.", finalPath.c_str());
      return HYDRA_FAILURE;
    }

    Result returnValue = HYDRA_SUCCESS;
    for (const auto& item : content.items())
    {
      if (auto permVarType = EvaluatePermutationVarType(item.key(), item.value(), m_logger))
      {
        // Handle permutation variable types - note: We keep parsing regardless of whether we were successful...
        switch (permVarType.value())
        {
          case json::value_t::boolean:
            if (RegisterBoolPermutationVar(permMgr, item.key(), item.value(), m_logger).Failed())
            {
              returnValue = HYDRA_FAILURE;
            }
            break;
          case json::value_t::number_integer:
            if (RegisterIntPermutationVar(permMgr, item.key(), item.value(), m_logger).Failed())
            {
              returnValue = HYDRA_FAILURE;
            }
            break;
          case json::value_t::array:
            if (RegisterEnumPermutationVar(permMgr, item.key(), item.value(), m_logger).Failed())
            {
              returnValue = HYDRA_FAILURE;
            }
            break;
        }
      }
    }

    if (returnValue.Succeeded())
    {
      Runtime::Log::Info(m_logger, "Successfully registered permutation variables from '%s'", finalPath.c_str());
    }
    else
    {
      Runtime::Log::Error(m_logger, "Failed to register permutation variables from '%s'", finalPath.c_str());
    }

    return returnValue;
  }



} // namespace Hydra::Tools
