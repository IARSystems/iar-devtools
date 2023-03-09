/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteModel.cpp
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/

#include "RteModel.h"

#include "RteGenerator.h"
#include "RteProject.h"
#include "CprjFile.h"

#include "RteFsUtils.h"
#include "XMLTree.h"

using namespace std;

//////////////////////////////////////////////////////////
RteModel::RteModel(RteItem* parent, PackageState packageState) :
  RteItem(parent),
  m_packageState(packageState),
  m_callback(NULL),
  m_bUseDeviceTree(true),
  m_filterContext(NULL)
{
  m_deviceTree = new RteDeviceItemAggregate("DeviceList", RteDeviceItem::VENDOR_LIST, NULL);
}


//////////////////////////////////////////////////////////
RteModel::RteModel(PackageState packageState) :
  RteItem(NULL),
  m_packageState(packageState),
  m_callback(NULL),
  m_bUseDeviceTree(false),
  m_filterContext(NULL)
{
  m_deviceTree = new RteDeviceItemAggregate("DeviceList", RteDeviceItem::VENDOR_LIST, NULL);
}


RteModel::~RteModel()
{
  RteModel::Clear();
  m_filterContext = NULL;
  delete m_deviceTree;
}

void RteModel::Clear()
{
  ClearModel();
}

void RteModel::ClearModel()
{
  ClearDevices();

  m_componentList.clear();
  m_apiList.clear();
  m_taxonomy.clear();
  m_bundles.clear();

  m_imageDescriptors.clear();
  m_layerDescriptors.clear();
  m_projectDescriptors.clear();
  m_solutionDescriptors.clear();

  list<RtePackage*>::iterator it;
  for (it = m_packageDuplicates.begin(); it != m_packageDuplicates.end(); it++) {
    delete *it;
  }
  m_packageDuplicates.clear();
  m_packages.clear();
  m_latestPackages.clear();

  RteItem::Clear();
}

void RteModel::ClearDevices()
{
  for (auto it = m_deviceVendors.begin(); it != m_deviceVendors.end(); it++) {
    delete it->second;
  }
  m_deviceVendors.clear();
  m_deviceTree->Clear();
  m_boards.clear();
}


 RteCallback* RteModel::GetCallback() const
 {
   return m_callback ? m_callback : RteCallback::GetGlobal();
 }

void RteModel::SetCallback(RteCallback* callback)
{
  m_callback = callback;
}

RtePackage* RteModel::GetPackage(const string& id) const
{
  auto it = m_packages.find(id);
  if (it != m_packages.end())
    return it->second;
  return NULL;
}


RtePackage* RteModel::GetLatestPackage(const string& id) const
{
  auto it = m_latestPackages.find(RtePackage::CommonIdFromId(id));
  if (it != m_latestPackages.end())
    return it->second;
  return NULL;
}

RtePackage* RteModel::GetAvailablePackage(const string& id) const
{
  RtePackage* pack = GetPackage(id);
  if (pack)
    return pack;
  pack = GetLatestPackage(id);
  if (!pack)
    return nullptr;
  string packVer = RtePackage::VersionFromId(id);
  if (VersionCmp::Compare(pack->GetVersionString(), packVer) > 0) {
    return pack;
  }
  return nullptr;
}


RtePackage* RteModel::GetPackage(const XmlItem& attr) const
{
  string commonId = RtePackage::GetPackageIDfromAttributes(attr, false);
  RtePackage* pack = GetLatestPackage(commonId);
  if (!pack)
    return NULL; // latest not found => none is found
  const string& versionRange = attr.GetAttribute("version");
  if (versionRange.empty()) {
    return pack; // version is not provided => the latest
  }
  if (VersionCmp::RangeCompare(pack->GetVersionString(), versionRange) == 0)
    return pack; // the latest matches the range

  for (auto itp = m_packages.begin(); itp != m_packages.end(); itp++) {
    pack = itp->second;
    if (pack->GetPackageID(false) != commonId)
      continue;
    if (VersionCmp::RangeCompare(pack->GetVersionString(), versionRange) == 0)
      return pack; // the latest matches the range
  }
  return NULL;
}

RteBoard* RteModel::FindBoard(const string& displayName) const
{
  const RteBoardMap& availableBoards = GetBoards();
  for (auto it = availableBoards.begin(); it != availableBoards.end(); it++) {
    RteBoard* b = it->second;
    if (b->GetDisplayName() == displayName) {
      return b;
    }
  }
  return nullptr;
}

void RteModel::GetCompatibleBoards(vector<RteBoard*>& boards, RteDeviceItem* device, bool bOnlyMounted) const
{
  if (!device) {
    return;
  }
  XmlItem ea;
  device->GetEffectiveAttributes(ea);
  const RteBoardMap& availableBoards = GetBoards();
  for (auto it = availableBoards.begin(); it != availableBoards.end(); it++) {
    RteBoard* b = it->second;
    if (b->HasCompatibleDevice(ea, bOnlyMounted)) {
      boards.push_back(b);
    }
  }
}


RteBoard* RteModel::FindCompatibleBoard(const string& displayName, RteDeviceItem* device, bool bOnlyMounted) const
{
  vector<RteBoard*> boards;
  GetCompatibleBoards(boards, device, bOnlyMounted);
  for (auto b : boards) {
    if (b->GetDisplayName() == displayName) {
      return b;
    }
  }
  return nullptr;
}

RteComponent* RteModel::GetComponent(const string& uniqueID) const
{
  // look in the APIs
  map<string, RteApi* >::const_iterator ita = m_apiList.find(uniqueID);
  if (ita != m_apiList.end()) {
    return ita->second;
  }

  // look in the components
  RteComponentMap::const_iterator itc = m_componentList.find(uniqueID);
  if (itc != m_componentList.end()) {
    return itc->second;
  }
  return NULL;
}

RteComponent* RteModel::GetComponent(RteComponentInstance* ci, bool matchVersion) const
{
  if (ci->IsApi() || matchVersion)
    return GetComponent(ci->GetID());

  string id = ci->GetComponentID(false);

  for (auto it = m_componentList.begin(); it != m_componentList.end(); it++) {
    RteComponent* c = it->second;
    if (c->GetComponentID(false) == id)
      return c;
  }
  return NULL;
}


RteApi* RteModel::GetApi(const map<string, string>& componentAttributes) const
{
  map<string, RteApi*>::const_iterator it;
  for (it = m_apiList.begin(); it != m_apiList.end(); it++) {
    RteApi* a = it->second;
    if (a && a->MatchApiAttributes(componentAttributes))
      return a;
  }
  return NULL;
}

RteApi* RteModel::GetApi(const string& id) const
{
  map<string, RteApi*>::const_iterator it = m_apiList.find(id);
  if (it != m_apiList.end()) {
    RteApi* a = it->second;
    return a;
  }
  return NULL;
}

RteBundle* RteModel::GetBundle(const string& id) const
{
  auto it = m_bundles.find(id);
  if (it != m_bundles.end())
    return it->second;
  return NULL;
}

RteBundle* RteModel::GetLatestBundle(const string& id) const
{
  for (auto it = m_bundles.begin(); it != m_bundles.end(); it++) {
    RteBundle* b = it->second;
    if (b->GetBundleID(false) == id)
      return b;
  }
  return NULL;
}


RteItem* RteModel::GetTaxonomyItem(const string& id) const
{
  map<string, RteItem*>::const_iterator it = m_taxonomy.find(id);
  if (it != m_taxonomy.end()) {
    return it->second;
  }
  return NULL;
}

const string& RteModel::GetTaxonomyDescription(const string& id) const
{
  RteItem* t = GetTaxonomyItem(id);
  if (t) {
    return t->GetText();
  }
  return EMPTY_STRING;
}


string RteModel::GetTaxonomyDoc(const string& id) const
{
  RteItem* t = GetTaxonomyItem(id);
  if (t) {
    return t->GetDocFile();
  }
  return EMPTY_STRING;
}


RteCondition* RteModel::GetCondition(const string& packageId, const string& conditionId) const
{
  RtePackage* package = GetPackage(packageId);
  if (package)
    return package->GetCondition(conditionId);
  return NULL;
}

RtePackage* RteModel::CreatePackage(const string& tag)
{
  return new RtePackage(this);
}

bool RteModel::ConstructPacks(XMLTreeElement* xmlTree, list<RtePackage*>& packs)
{
  if (!xmlTree) {
    return false;
  }
  bool success = true;
  const list<XMLTreeElement*>& docs = xmlTree->GetChildren();
  for (auto it = docs.begin(); it != docs.end(); it++) {

    XMLTreeElement* xmlElement = *it;
    RtePackage* package = ConstructPack(xmlElement);
    if (package) {
      packs.push_back(package);
    } else {
      success = false;
    }
  }
  return success;
}

RtePackage* RteModel::ConstructPack(XMLTreeElement* xmlTreeDoc)
{
  if (!xmlTreeDoc || !xmlTreeDoc->IsValid()) {
    return nullptr;
  }

  RtePackage* package = CreatePackage(xmlTreeDoc->GetTag());
  bool ok = package->Construct(xmlTreeDoc);
  if (ok) {
    package->SetPackageState(GetPackageState());
    GetCallback()->PackProcessed(package->GetID(), true);
    return package;
  }
  GetCallback()->PackProcessed(xmlTreeDoc->GetRootFileName(), false);
  const list<string>& errors = package->GetErrors();
  m_errors.insert(m_errors.end(), errors.begin(), errors.end());
  delete package;
  return nullptr;
}


bool RteModel::Construct(XMLTreeElement* xmlTree)
{
  list<RtePackage*> packs;

  if(!ConstructPacks(xmlTree, packs)) {
    return false;
  }
  InsertPacks(packs);
  return true;
}

void RteModel::InsertPacks(const list<RtePackage*>& packs)
{
  for (auto it = packs.begin(); it != packs.end(); it++) {
    InsertPack(*it);
  }
  FillComponentList(NULL); // no device package yet
  FillDeviceTree();
}

void RteModel::InsertPack(RtePackage* package)
{
  // check for duplicates
  const string& id = package->GetID();
  RtePackage* insertedPack = GetPackage(id);
  if (insertedPack) {
    string pdscPath = RteFsUtils::MakePathCanonical(package->GetAbsolutePackagePath());
    if (pdscPath.find(m_rtePath) == 0) { // regular installed pack => error
    // duplicate, keept it in a temporary collection till validate;
      m_packageDuplicates.push_back(package);
      return;
    }
    string insertedPdscPath = RteFsUtils::MakePathCanonical(insertedPack->GetAbsolutePackagePath());
    if (insertedPdscPath.find(m_rtePath) == string::npos) { // inserted pack is also from outside => error
      m_packageDuplicates.push_back(package);
      return;
    }
  }

  // OK, add to normal children
  AddItem(package);
  // add to general sorted package map
  m_packages[id] = package;
  // add to latest package map
  const string& commonId = package->GetCommonID();
  RtePackage* p = GetLatestPackage(commonId);
  if (!p || p == insertedPack || VersionCmp::Compare(package->GetVersionString(), p->GetVersionString()) > 0) {
    m_latestPackages[commonId] = package;
  }
  if (insertedPack) {
    // remove inserted pack as replaced
    RemoveItem(insertedPack);
    delete insertedPack;
  }
}


bool RteModel::Validate()
{
  m_bValid = RteItem::Validate();
  if (!m_errors.empty())
    m_bValid = false;
  if (!m_packageDuplicates.empty()) {
    m_bValid = false;
    list<RtePackage*>::iterator it;
    for (it = m_packageDuplicates.begin(); it != m_packageDuplicates.end(); it++) {
      RtePackage* duplicate = *it;
      RtePackage* orig = GetPackage(duplicate->GetID());

      string msg = duplicate->GetPackageFileName();
      msg += ": warning #500: pack '" + duplicate->GetID() + "' is already defined in file " + orig->GetPackageFileName();
      msg += " - duplicate is ignored";
      m_errors.push_back(msg);
      delete *it;
    }

    m_packageDuplicates.clear();
  }
  return m_bValid;
}

RtePackage* RteModel::FilterModel(RteModel* globalModel, RtePackage* devicePackage)
{
  Clear();

  // first add all latest packs
  set<string> latestPackIds;
  const RtePackageMap& latestPackages = globalModel->GetLatestPackages();
  for ( auto& [key, pack] : latestPackages) {
    if (pack) {
      latestPackIds.insert(pack->GetID());
    }
  }
  m_packageFilter.SetLatestInstalledPacks(latestPackIds); // filter requires global latests

  const RtePackageMap& allPacks = globalModel->GetPackages();
  for (auto itp = allPacks.begin(); itp != allPacks.end(); itp++) {
    RtePackage* pack = itp->second;
    const string& id = itp->first;
    if (!m_packageFilter.IsPackageFiltered(pack))
      continue;

    if (m_packageFilter.IsPackageSelected(pack)) {
      m_packages[id] = pack; //directly add to map
    }
    const string& commonId = pack->GetCommonID();
    RtePackage* p = GetLatestPackage(commonId);
    if (!p || VersionCmp::Compare(pack->GetVersionString(), p->GetVersionString()) > 0) {
      m_latestPackages[commonId] = pack;
    }
  }
  if (devicePackage && !m_packageFilter.IsPackageFiltered(devicePackage)) {
    const string& commonId = devicePackage->GetCommonID();
    devicePackage = GetLatestPackage(commonId);
  }

  // add latest packs that pass filter, but not added yet
  for (auto itp = m_latestPackages.begin(); itp != m_latestPackages.end(); itp++) {
    RtePackage* pack = itp->second;
    const string& id = pack->GetID();
    if (m_packages.find(id) == m_packages.end())
      m_packages[itp->first] = pack;
  }

  FillComponentList(devicePackage);
  FillDeviceTree();
  return devicePackage; // now effective
}

void RteModel::AddItemsFromPack(RtePackage* pack)
{
  RteItem* taxonomy = pack->GetTaxonomy();
  if (taxonomy) {
    // fill in taxonomy
    const list<RteItem*>& children = taxonomy->GetChildren();
    for (auto it = children.begin(); it != children.end(); it++) {
      RteItem* t = *it;
      string id = t->GetTaxonomyDescriptionID();
      if (GetTaxonomyItem(id) == NULL && IsFiltered(t)) {// do not overwrite the newest
        m_taxonomy[id] = t;
      }
    }
  }
  // images
  AddPackItemsToList(pack->GetImageDescriptors(), m_imageDescriptors);
  // layers
  AddPackItemsToList(pack->GetLayerDescriptors(), m_layerDescriptors);
  // projects
  AddPackItemsToList(pack->GetProjectDescriptors(), m_projectDescriptors);
  // projects
  AddPackItemsToList(pack->GetSolutionDescriptors(), m_solutionDescriptors);

  // fill api and component list
  pack->InsertInModel(this);
}

void RteModel::AddPackItemsToList(const std::list<RteItem*>& srcCollection, std::list<RteItem*>& dstCollection)
{
  for (auto item : srcCollection) {
    if (IsFiltered(item)) {
      dstCollection.push_back(item);
    }
  }
}

bool RteModel::IsFiltered(RteItem* item) {
  if (!item)
    return false;
  if (!GetFilterContext())
    return true;
  if (item->Evaluate(GetFilterContext()) < FULFILLED)
    return false;
  return true;
}


void RteModel::FillComponentList(RtePackage* devicePackage)
{
  m_componentList.clear();
  m_taxonomy.clear();
  m_apiList.clear();

  // first process DFP - it has precedence
  if (devicePackage) {
    AddItemsFromPack(devicePackage);
  }

  // evaluate dominate packages first
  RtePackageMap::iterator itp;
  for (itp = m_packages.begin(); itp != m_packages.end(); itp++) {
    RtePackage* package = itp->second;
    if (package == devicePackage)
      continue;
    if (package->IsDeprecated())
      continue;
    if (package->IsDominating()) {
      AddItemsFromPack(itp->second);
    }
  }

  // evaluated sorted collection, deprecated packs in the second run
  bool bHasDeprecated = false;
  for (itp = m_packages.begin(); itp != m_packages.end(); itp++) {
    RtePackage* package = itp->second;
    if (package->IsDeprecated()) {
      bHasDeprecated = true;
      continue;
    }
    if (package->IsDominating())
      continue;
    if (package == devicePackage)
      continue;
    AddItemsFromPack(package);
  }
  if (!bHasDeprecated)
    return;
  for (itp = m_packages.begin(); itp != m_packages.end(); itp++) {
    RtePackage* package = itp->second;
    if (!package->IsDeprecated())
      continue;
    if (package == devicePackage)
      continue;
    AddItemsFromPack(package);
  }

}

void RteModel::InsertComponent(RteComponent* c)
{
  if (!c)
    return;
  const string& id = c->GetID();
  if (c->IsApi()) {
    RteApi* a = dynamic_cast<RteApi*>(c);
    if (IsFiltered(a) && IsApiDominatingOrNewer(a)) {
      m_apiList[id] = a;
    }
  } else {
    // do not allow duplicates (filtering is performed later)
    if (GetComponent(id))
      return;
    m_componentList[id] = c;
  }
}

bool RteModel::IsApiDominatingOrNewer(RteApi* a) {
  const string& id = a->GetID();
  RteApi* aExisting = GetApi(id);
  if (!aExisting)
    return true;
  bool existingDominating = aExisting->GetPackage()->IsDominating();
  bool packageDominating = a->GetPackage()->IsDominating();
  if (packageDominating && !existingDominating)
    return true; // use new api anyway since it is dominating
  if (!packageDominating && existingDominating)
    return false;
  return VersionCmp::Compare(aExisting->GetVersionString(), a->GetVersionString()) < 0;
}


void RteModel::InsertBundle(RteBundle* b)
{
  if (!b)
    return;
  const string& id = b->GetID();
  RteBundle* inserted = GetBundle(id);
  if (inserted) {

    // insert bundle with most information (description and doc)
    if (b->GetDocFile().empty() && b->GetDescription().empty())
      return;

    if (!inserted->GetDocFile().empty()) {
      // ensure bundle has description
      if (inserted->GetDescription().empty())
        inserted->SetText(b->GetDescription());
      return;
    }
    if (b->GetDescription().empty())
      b->SetText(inserted->GetDescription());
  }
  m_bundles[id] = b;
}

RteDeviceVendor* RteModel::FindDeviceVendor(const string& vendor) const
{
  auto it = m_deviceVendors.find(vendor);
  if (it != m_deviceVendors.end())
    return it->second;
  return NULL;

}

RteDeviceVendor* RteModel::EnsureDeviceVendor(const string& vendor)
{
  if (vendor.empty())
    return NULL;
  RteDeviceVendor* dv = FindDeviceVendor(vendor);
  if (dv)
    return dv;
  dv = new RteDeviceVendor(vendor);
  m_deviceVendors[vendor] = dv;
  return dv;
}

bool RteModel::AddDeviceItem(RteDeviceItem* item)
{
  if (!item)
    return false;
  string vendor = item->GetVendorName();
  RteDeviceVendor* dv = EnsureDeviceVendor(vendor);
  return dv->AddDeviceItem(item);
}


void RteModel::GetDevices(list<RteDevice*>& devices, const string& namePattern, const string& vendor, RteDeviceItem::TYPE depth) const
{
  if (namePattern.empty() || namePattern.find_first_of("*?[") != string::npos) {
    if (IsUseDeviceTree()) {
      m_deviceTree->GetDevices(devices, namePattern, vendor, depth); // pattern match
      return;
    }
    for (auto it = m_deviceVendors.begin(); it != m_deviceVendors.end(); it++) {
      RteDeviceVendor* dv = it->second;
      dv->GetDevices(devices, namePattern);
    }
    return;
  }
  RteDevice* d = GetDevice(namePattern, vendor);
  if (d)
    devices.push_back(d);
}

RteDevice* RteModel::GetDevice(const string& deviceName, const string& vendor) const
{
  if (!vendor.empty()) {
    string vendorName = DeviceVendor::GetCanonicalVendorName(vendor);
    RteDeviceVendor* dv = FindDeviceVendor(vendorName);
    if (dv) {
      RteDevice* d = dv->GetDevice(deviceName);
      if (d)
        return d;

    }
  } else {
    for (auto it = m_deviceVendors.begin(); it != m_deviceVendors.end(); it++) {
      RteDeviceVendor* dv = it->second;
      RteDevice* d = dv->GetDevice(deviceName);
      if (d)
        return d;
    }
  }
  if (IsUseDeviceTree())
    return dynamic_cast<RteDevice*>(m_deviceTree->GetDeviceItem(deviceName, vendor));
  return NULL;
}

int RteModel::GetDeviceCount() const
{
  int count = 0;
  for (auto it = m_deviceVendors.begin(); it != m_deviceVendors.end(); it++) {
    RteDeviceVendor* dv = it->second;
    count += dv->GetCount();
  }
  return count;

}
int RteModel::GetDeviceCount(const string& vendor) const
{
  RteDeviceVendor* dv = FindDeviceVendor(vendor);
  if (dv)
    return dv->GetCount();
  return 0;
}


RteDeviceItemAggregate* RteModel::GetDeviceAggregate(const string& deviceName, const string& vendor) const {
  return (m_deviceTree->GetDeviceAggregate(deviceName, vendor));
}

RteDeviceItemAggregate* RteModel::GetDeviceItemAggregate(const string& name, const string& vendor) const {
  return (m_deviceTree->GetDeviceItemAggregate(name, vendor));
}

// fill device and board information
void RteModel::FillDeviceTree()
{
  ClearDevices();

  bool bHasDeprecated = false;
  // use only latest packages
  RtePackageMap::iterator it;
  for (it = m_latestPackages.begin(); it != m_latestPackages.end(); it++) {
    RtePackage* package = it->second;
    if (!package)
      continue;
    if (package->IsDeprecated()) {
      bHasDeprecated = true;
      continue;
    }
    FillDeviceTree(package);
  }

  if (!bHasDeprecated)
    return;
  for (it = m_latestPackages.begin(); it != m_latestPackages.end(); it++) {
    RtePackage* package = it->second;
    if (!package)
      continue;
    if (!package->IsDeprecated()) {
      continue;
    }
    FillDeviceTree(package);
  }
}

void RteModel::FillDeviceTree(RtePackage* package)
{
  RteDeviceFamilyContainer* families = package->GetDeviceFamiles();
  if (families) {
    const list<RteItem*>& children = families->GetChildren();
    for (auto itfam = children.begin(); itfam != children.end(); itfam++) {
      RteDeviceFamily* fam = dynamic_cast<RteDeviceFamily*>(*itfam);
      if (fam) {
        bool bInserted = AddDeviceItem(fam);
        if (!bInserted)
          continue;
        if (!IsUseDeviceTree()) // additionally add device info as a tree
          continue;
        m_deviceTree->AddDeviceItem(fam);
      }
    }
  }
  RteItem* boards = package->GetBoards();
  if (boards) {
    const list<RteItem*>& children = boards->GetChildren();
    for (auto it = children.begin(); it != children.end(); it++) {
      RteBoard* b = dynamic_cast<RteBoard*>(*it);
      if (b == 0)
        continue;
      const string& id = b->GetID();
      if (m_boards.find(id) == m_boards.end()) {
        m_boards[id] = b;
      }
    }
  }
}


void RteModel::GetBoardBooks(map<string, string>& books, const string& device, const string& vendor) const
{
  if (m_boards.empty())
    return;
  RteDevice* d = GetDevice(device, vendor);
  if (!d)
    return;
  XmlItem ea;
  d->GetEffectiveAttributes(ea);
  GetBoardBooks(books, ea.GetAttributes());
}

void RteModel::GetBoardBooks(map<string, string>& books, const map<string, string>& deviceAttributes) const
{
  if (m_boards.empty())
    return;
  for (auto it = m_boards.begin(); it != m_boards.end(); it++) {
    RteBoard* b = it->second;
    if (b->HasCompatibleDevice(deviceAttributes)) {
      b->GetBooks(books);
    }
  }
}

RteGlobalModel::RteGlobalModel() :
  RteModel(NULL, PackageState::PS_INSTALLED),
  m_nActiveProjectId(-1)
{
}
RteGlobalModel::~RteGlobalModel()
{
  RteGlobalModel::Clear();
}

void RteGlobalModel::Clear()
{
  ClearProjects();
  ClearModel();
}

void RteGlobalModel::ClearModel()
{
  ClearProjectTargets();
  RteModel::ClearModel();
}


void RteGlobalModel::SetCallback(RteCallback* callback)
{
  RteModel::SetCallback(callback);
  for (auto it = m_projects.begin(); it != m_projects.end(); it++) {
    RteProject* project = it->second;
    project->SetCallback(callback);
  }
}

// projects
////////////////////////////////////////////////

int RteGlobalModel::GenerateProjectId()
{
  int id = 1;
  int n = (int)(m_projects.size() + 2);
  for (; id < n ; id++) {
    if (!GetProject(id))
      break;
  }
  return id;
}

RteProject* RteGlobalModel::GetProject(int id) const
{
  auto it = m_projects.find(id);
  if (it != m_projects.end())
    return it->second;
  return nullptr;
}

RteProject* RteGlobalModel::AddProject(int id, RteProject* project)
{
  if (id <= 0) {
    id = GenerateProjectId();
  }
  if (!project) {
    project = GetProject(id);
    if (!project) {
      project = new RteProject();
    }
  }
  project->SetProjectId(id);
  project->SetModel(this);
  project->SetCallback(m_callback);
  m_projects[id] = project;
  return project;
}

void RteGlobalModel::DeleteProject(int id)
{
  auto it = m_projects.find(id);
  if (it != m_projects.end()) {
    delete it->second;
    m_projects.erase(it);
    if (id == m_nActiveProjectId)
      m_nActiveProjectId = -1;
  }
}

void RteGlobalModel::ClearProjects()
{
  for (auto it = m_projects.begin(); it != m_projects.end(); it++) {
    delete it->second;
  }
  m_projects.clear();
  m_nActiveProjectId = -1;
}


void RteGlobalModel::ClearProjectTargets(int id)
{
  for (auto it = m_projects.begin(); it != m_projects.end(); it++) {
    if (id <= 0 || id == it->first) {
      RteProject* project = it->second;
      project->ClearTargets();
    }
  }
}

// End of RteModel.cpp
