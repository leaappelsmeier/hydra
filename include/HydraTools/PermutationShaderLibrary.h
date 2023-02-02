#pragma once

#include <HydraRuntime/PermutationSets.h>
#include <HydraTools/PermutationShader.h>
#include <map>
#include <mutex>
#include <string>

namespace Hydra::Runtime
{
  struct ILoggingInterface;
}

namespace Hydra::Tools
{
  class FileCache;
  class FileLocator;
  class TextSectionizer;

  /// A shader library object is used to load permutation shaders (and their dependencies) and generate their permutations.
  ///
  /// Note that although this is very useful infrastructure, it is not mandatory to go through this code, to use the Runtime functionality.
  /// Depending on how your engine works, what file formats you want, etc, you may prefer to do all this yourself.
  class PermutationShaderLibrary
  {
  public:
    PermutationShaderLibrary();

    /// Sets the logger implementation to use for error reporting. Optional, but useful to get more information about issues.
    void SetLogger(Runtime::ILoggingInterface* logger);

    /// Sets the file cache implementation to use. This is mandatory to set before loading any shader.
    void SetFileCache(FileCache* cache);

    /// Sets the file locator implementation to use. This is mandatory to set before loading any shader.
    void SetFileLocator(FileLocator* locator);

    /// Attempts to return a previously loaded shader. Returns nullptr, if no shader with the given path is loaded yet.
    const PermutationShader* GetLoadedPermutationShader(std::string_view path) const;

    /// Attempts to load a shader. Returns nullptr if shader loading failed. Consult the log for details.
    const PermutationShader* LoadPermutationShader(std::string_view path);

    /// Returns the set of all permutation variables that appear in conditions in this shader (including imports).
    ///
    /// This set should be strictly contained in the 'allowed' set of permutation variables.
    /// Otherwise, the user has to add variable names to the allowed set in the [PERMUTATIONS] section.
    ///
    /// Note that this information is mainly meant for validation and debugging.
    void GetAllUsedPermutationVariables(const PermutationShader& shader, std::set<std::string>& allUsedVariables) const;

    /// Returns the set of all files that need to be read to get the full information about this permutation shader.
    ///
    /// This includes the shader file itself, all imported shaders, and all directly and indirectly #include'd files.
    ///
    /// This information can be useful to determine whether any dependency has been modified.
    void GetAllReferencedFiles(const PermutationShader& shader, std::set<std::string>& allReferencedFiles) const;

    /// Returns the user specified configuration, which permutation variable may be permuted freely and which is supposed to have a fixed value.
    ///
    /// In the [PERMUTATIONS] section of the shader file, users have to declare every permutation variable that is
    /// directly or indirectly (including imports) used by a shader.
    /// If the variable is simply mentioned by name or assigned '*', this means that it participates in shader permutation generation.
    /// If the variable is assigned a concrete value like "A = true", this means that it will always use this fixed value and even if
    /// the runtime sets a different value for this variable, it will always generate the same code.
    ///
    /// If a variable is used by a shader (even indirectly from an import) but not mentioned in the top-level [PERMUTATIONS] section,
    /// this is an error, because it is then unclear whether the shader author was even aware of the existence (and usage) of that variable.
    /// All usages of permutation variables have to be agreed to at the top level by specifying them in the [PERMUTATIONS] section.
    void GetAllowedVariablePermutations(const PermutationShader& shader, std::map<std::string, std::string>& allowedVariableValues) const;

    /// Generates the text permutation of one of the shader sections (e.g. vertex shader or pixel shader code).
    ///
    /// This is supposed to be used when a certain shader permutation needs to be compiled and the actual source code is needed.
    std::optional<std::string> GeneratePermutedShaderCode(const PermutationShader& shader, ShaderFileSection::Enum stage, const PermutationVariableValues& permutationVariables) const;

    /// Creates the PermutationVariableSet for the given shader. This should be done once for each shader and the result stored.
    ///
    /// The PermutationVariableSet specifies which permutation variables the shader 'exposes', ie allows to be permuted.
    /// This is used during shader permutation selection to determine which variables even to consider.
    Runtime::PermutationVariableSet CreatePermutationVariableSet(const PermutationShader& shader, const Runtime::PermutationManager& permutationManager);

    /// Fills out the map of variable names and values needed to generate the selected shader permutation.
    ///
    /// The runtime returns a PermutationVariableSelection that specifies which shader permutation is needed.
    /// To generate the source code for that particular permutation, call GeneratePermutedShaderCode().
    /// This function allows you to easily fill out the PermutationVariableValues for that call.
    Runtime::Result SetupVariableValuesForPermutationSelection(PermutationVariableValues& variables, const PermutationShader& shader, const Runtime::PermutationManager& manager, const Runtime::PermutationVariableSelection& selection);

  private:
    Runtime::Result ParseShaderImports(PermutationShader& shader, std::string_view imports) const;
    Runtime::Result LoadShaderImports(PermutationShader& shader);
    Runtime::Result ParsePermutationConfiguration(std::map<std::string, std::string>& allowedPermutations, std::string_view permutations) const;
    Runtime::Result ParseShaderFile(PermutationShader& shader, const std::string& content);
    Runtime::Result ValidateShader(PermutationShader& shader) const;

    Runtime::Result SetupVariableValuesWithNeededEnumValues(PermutationVariableValues& variables, const PermutationShader& shader, const Runtime::PermutationManager& manager, const std::map<std::string, std::string>& allowedValues);
    Runtime::Result SetupVariableValuesWithSelectionValues(PermutationVariableValues& variables, const Runtime::PermutationVariableSelection& selection) const;
    Runtime::Result SetupVariableValuesWithFixedValues(PermutationVariableValues& variables, const PermutationShader& shader, const Runtime::PermutationManager& manager, const std::map<std::string, std::string>& allowedValues);
    void ConfigureTextSectionizer(TextSectionizer& sectionizer) const;

    mutable std::recursive_mutex m_mutex;
    Runtime::ILoggingInterface* m_logger = nullptr;
    FileCache* m_fileCache = nullptr;
    FileLocator* m_fileLocator = nullptr;
    std::map<std::string, PermutationShader> m_loadedShaders;
  };
} // namespace Hydra::Tools
