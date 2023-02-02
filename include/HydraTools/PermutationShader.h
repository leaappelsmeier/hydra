#pragma once

#include <HydraTools/PermutableText.h>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Hydra::Tools
{
  struct ShaderFileSection
  {
    enum Enum
    {
      VertexShader,   // [VERTEX_SHADER]
      HullShader,     // [HULL_SHADER]
      DomainShader,   // [DOMAIN_SHADER]
      GeometryShader, // [GEOMETRY_SHADER]
      PixelShader,    // [PIXEL_SHADER]
      ComputeShader,  // [COMPUTE_SHADER]

      User1, // Custom: Configure SectionNames[ShaderFileSection::User1]
      User2, // Custom: Configure SectionNames[ShaderFileSection::User2]
      User3, // Custom: Configure SectionNames[ShaderFileSection::User3]
      User4, // Custom: Configure SectionNames[ShaderFileSection::User4]
      User5, // Custom: Configure SectionNames[ShaderFileSection::User5]
      User6, // Custom: Configure SectionNames[ShaderFileSection::User6]
      User7, // Custom: Configure SectionNames[ShaderFileSection::User7]
      User8, // Custom: Configure SectionNames[ShaderFileSection::User8]

      MAX_SECTIONS
    };

    static const char* SectionNames[MAX_SECTIONS];
  };

  /// Stores all the information about one loaded shader file.
  ///
  /// This includes the sources for the different shader stages,
  /// imports to other shaders, which files were referenced in #include statements,
  /// and which permutation variables are used.
  ///
  /// Note that this is only provided for convenience.
  /// You can use the Runtime infrastructure entirely without using the Tools code.
  /// If you prefer to use different file formats, or want to use different means to
  /// create the shader permutations (e.g. you want a more powerful system for stitching text fragments together)
  /// you can use your very own implementation as well.
  struct PermutationShader
  {
    // The normalized path to the file from which the shader data was loaded.
    std::string m_normalizedPath;

    // All the other shader files that were pulled in via 'import' statements at the start of the file.
    std::vector<std::string> m_imports;

    // All permutation variables that were mentioned in the sources of this shader (excluding imports).
    std::vector<std::string> m_usedPermutationVariables;

    // All files that needed to be read (excluding imports). These were mostly referenced by #include statements.
    std::set<std::string> m_referencedFiles;

    // The permutation variables that were mentioned in the '[PERMUTATIONS]' section.
    // If a variable was declared as 'VAR = value' the second string is the value, and this means that the variable should always
    // have this fixed value and not participate in permutation selection.
    // The variable can also be declared with no value at all: 'VAR' or with a * value 'VAR = *'.
    // This indicates that the variable may take on any of it's registered values and thus participates in generating different shader permutations.
    std::map<std::string, std::string> m_allowedVariablePermutations;

    // The text of each section in the shader file. These can be permuted.
    PermutableText m_shaderSection[ShaderFileSection::MAX_SECTIONS];
  };
} // namespace Hydra::Tools
