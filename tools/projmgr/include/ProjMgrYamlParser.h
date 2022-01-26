/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROJMGRYAMLPARSER_H
#define PROJMGRYAMLPARSER_H

#include "ProjMgrParser.h"
#include "yaml-cpp/yaml.h"

/**
  * @brief YAML key definitions
*/
static constexpr const char* YAML_ADDPATHS = "add-paths";
static constexpr const char* YAML_BOARD = "board";
static constexpr const char* YAML_BUILDTYPES = "build-types";
static constexpr const char* YAML_CATEGORY = "category";
static constexpr const char* YAML_CONSUMES = "consumes";
static constexpr const char* YAML_COMPILER = "compiler";
static constexpr const char* YAML_COMPONENT = "component";
static constexpr const char* YAML_COMPONENTS = "components";
static constexpr const char* YAML_DEBUG = "debug";
static constexpr const char* YAML_DEFINES = "defines";
static constexpr const char* YAML_DELPATHS = "del-paths";
static constexpr const char* YAML_DEPENDS = "depends";
static constexpr const char* YAML_DESCRIPTION = "description";
static constexpr const char* YAML_DEVICE = "device";
static constexpr const char* YAML_RTEDIRS = "rtedirs";
static constexpr const char* YAML_RTEDIR = "rtedir";
static constexpr const char* YAML_ENDIAN = "endian";
static constexpr const char* YAML_FILE = "file";
static constexpr const char* YAML_FILES = "files";
static constexpr const char* YAML_FORTYPE = "for-type";
static constexpr const char* YAML_FPU = "fpu";
static constexpr const char* YAML_GROUP = "group";
static constexpr const char* YAML_GROUPS = "groups";
static constexpr const char* YAML_INTERFACES = "interfaces";
static constexpr const char* YAML_LAYER = "layer";
static constexpr const char* YAML_LAYERS = "layers";
static constexpr const char* YAML_MISC = "misc";
static constexpr const char* YAML_MISC_ASM = "ASM";
static constexpr const char* YAML_MISC_C = "C";
static constexpr const char* YAML_MISC_COMPILER = "compiler";
static constexpr const char* YAML_MISC_CPP = "CPP";
static constexpr const char* YAML_MISC_C_CPP = "C*";
static constexpr const char* YAML_MISC_LIB = "Lib";
static constexpr const char* YAML_MISC_LINK = "Link";
static constexpr const char* YAML_NAME = "name";
static constexpr const char* YAML_NOTFORTYPE = "not-for-type";
static constexpr const char* YAML_OPTIMIZE = "optimize";
static constexpr const char* YAML_OUTPUTTYPE = "output-type";
static constexpr const char* YAML_PACKAGE = "package";
static constexpr const char* YAML_PACKAGES = "packages";
static constexpr const char* YAML_PROCESSOR = "processor";
static constexpr const char* YAML_PROJECT = "project";
static constexpr const char* YAML_PROJECTS = "projects";
static constexpr const char* YAML_PROVIDES = "provides";
static constexpr const char* YAML_SOLUTION = "solution";
static constexpr const char* YAML_TARGETTYPES = "target-types";
static constexpr const char* YAML_TRUSTZONE = "trustzone";
static constexpr const char* YAML_TYPE = "type";
static constexpr const char* YAML_UNDEFINES = "undefines";
static constexpr const char* YAML_WARNINGS = "warnings";

/**
  * @brief projmgr parser yaml implementation class, directly coupled to underlying yaml-cpp external library
*/
class ProjMgrYamlParser {
public:
  /**
   * @brief class constructor
  */
  ProjMgrYamlParser(void);

  /**
   * @brief class destructor
  */
  ~ProjMgrYamlParser(void);

  /**
   * @brief parse csolution
   * @param input csolution.yml file
   * @param reference to store parsed csolution item
  */
  bool ParseCsolution(const std::string& input, CsolutionItem& csolution);

  /**
   * @brief parse cproject
   * @param input cproject.yml file
   * @param reference to store parsed cproject item
  */
  bool ParseCproject(const std::string& input, CsolutionItem& csolution, std::map<std::string, CprojectItem>& cprojects, bool single);

  /**
   * @brief parse clayer
   * @param input clayer.yml file
   * @param reference to store parsed clayer item
  */
  bool ParseClayer(const std::string& input, std::map<std::string, ClayerItem>& clayers);

protected:
  void ParseMisc(const YAML::Node& parent, std::vector<MiscItem>& misc);
  void ParsePackages(const YAML::Node& parent, std::vector<std::string>& packages);
  void ParseProcessor(const YAML::Node& parent, ProcessorItem& processor);
  void ParseString(const YAML::Node& parent, const std::string& key, std::string& value);
  void ParseString(YAML::Node node, std::string& value);
  void ParseVector(const YAML::Node& parent, const std::string& key, std::vector<std::string>& value);
  void ParseVectorOrString(const YAML::Node& parent, const std::string& key, std::vector<std::string>& value);
  void ParseBuildType(const YAML::Node& parent, BuildType& buildType);
  void ParseDepends(const YAML::Node& parent, std::vector<std::string>& depends);
  void ParseTargetType(const YAML::Node& parent, TargetType& targetType);
  void ParseBuildTypes(const YAML::Node& parent, std::map<std::string, BuildType>& buildTypes);
  void ParseTargetTypes(const YAML::Node& parent, std::map<std::string, TargetType>& targetTypes);
  bool ParseContexts(const YAML::Node& parent, CsolutionItem& contexts);
  bool ParseRteDirs(const YAML::Node& parent, std::vector<RteDirItem>& dirs);
  bool ParseComponents(const YAML::Node& parent, std::vector<ComponentItem>& components);
  bool ParseFiles(const YAML::Node& parent, std::vector<FileNode>& files);
  bool ParseGroups(const YAML::Node& parent, std::vector<GroupNode>& groups);
  bool ParseLayers(const YAML::Node& parent, std::vector<LayerItem>& layers);
  bool ParseTypeFilter(const YAML::Node& parent, TypeFilter& type);
  bool ParseTypePair(std::vector<std::string>& vec, std::vector<TypePair>& typeVec);
  bool GetTypes(const std::string& type, std::string& buildType, std::string& targetType);
  void PushBackUniquely(std::vector<std::string>& vec, const std::string& value);
  bool ValidateCsolution(const std::string& input, const YAML::Node& root);
  bool ValidateCproject(const std::string& input, const YAML::Node& root);
  bool ValidateClayer(const std::string& input, const YAML::Node& root);
  bool ValidateKeys(const std::string& input, const YAML::Node& parent, const std::set<std::string>& keys);
  bool ValidateSequence(const std::string& input, const YAML::Node& parent, const std::string& seqKey);
  bool ValidateMapping(const std::string& input, const YAML::Node& parent, const std::string& seqKey);

};

#endif  // PROJMGRYAMLPARSER_H
