// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ProfileManager.h"
#include "Core/Core.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

namespace InputCommon
{
namespace
{
void DisplayLoadMessage(const InputProfile profile,
                        const ControllerEmu::EmulatedController& controller)
{
  Core::DisplayMessage("Loading game specific input profile '" + profile.GetRelativePathToRoot() +
                           "' for device '" + controller.GetName() + "'",
                       6000);
}
}  // namespace
ProfileManager::ProfileManager(ControllerEmu::EmulatedController* owning_controller)
    : m_controller(owning_controller)
{
}

ProfileManager::~ProfileManager() = default;

void ProfileManager::NextProfile()
{
  m_profile_index++;
  m_profile = m_container->GetProfile(m_profile_index);
  if (m_profile)
  {
    m_profile->Apply(m_controller);
    DisplayLoadMessage(*m_profile, *m_controller);
  }
}

void ProfileManager::PreviousProfile()
{
  m_profile_index--;
  m_profile = m_container->GetProfile(m_profile_index);
  if (m_profile)
  {
    m_profile->Apply(m_controller);
    DisplayLoadMessage(*m_profile, *m_controller);
  }
}

void ProfileManager::NextProfileForGame()
{
  m_profile_index++;
  m_profile = m_container->GetProfile(m_profile_index, DeviceProfileContainer::Direction::Forward,
                                      m_profile_search_filter);
  if (m_profile)
  {
    m_profile->Apply(m_controller);
    DisplayLoadMessage(*m_profile, *m_controller);
  }
}

void ProfileManager::PreviousProfileForGame()
{
  m_profile_index--;
  m_profile = m_container->GetProfile(m_profile_index, DeviceProfileContainer::Direction::Backward,
                                      m_profile_search_filter);
  if (m_profile)
  {
    m_profile->Apply(m_controller);
    DisplayLoadMessage(*m_profile, *m_controller);
  }
}

void ProfileManager::SetProfile(int profile_index)
{
  m_profile_index = profile_index;
  m_profile = m_container->GetProfile(m_profile_index);
  if (m_profile)
  {
    m_profile->Apply(m_controller);
    DisplayLoadMessage(*m_profile, *m_controller);
  }
}

void ProfileManager::SetProfile(const std::string& path)
{
  m_profile_index = m_container->GetIndexFromPath(path);
  if (m_profile_index == -1)
  {
    m_profile = {};
  }
  else
  {
    m_profile = m_container->GetProfile(m_profile_index);
  }

  if (m_profile)
  {
    m_profile->Apply(m_controller);
    DisplayLoadMessage(*m_profile, *m_controller);
  }
}

void ProfileManager::SetProfileForGame(int profile_index)
{
  m_profile_index = profile_index;
  m_profile = m_container->GetProfile(m_profile_index, DeviceProfileContainer::Direction::Forward,
                                      m_profile_search_filter);
  if (m_profile)
  {
    m_profile->Apply(m_controller);
    DisplayLoadMessage(*m_profile, *m_controller);
  }
}

const std::optional<InputProfile>& ProfileManager::GetProfile() const
{
  return m_profile;
}

void ProfileManager::ClearProfile()
{
  m_profile.reset();
  m_profile_index = -1;
}

void ProfileManager::SetDeviceProfileContainer(DeviceProfileContainer* container)
{
  m_container = container;
}

int ProfileManager::GetProfileIndex()
{
  // Sync up the profile index if a profile exists
  if (m_profile)
  {
    m_profile_index = m_container->GetIndexFromPath(m_profile->GetAbsolutePath());
  }

  return m_profile_index;
}

void ProfileManager::AddToProfileFilter(const std::string& path)
{
  m_profile_search_filter.insert(path);
}

void ProfileManager::ClearProfileFilter()
{
  m_profile_search_filter.clear();
}

std::size_t ProfileManager::ProfilesInFilter() const
{
  return m_profile_search_filter.size();
}

}  // namespace InputCommon
