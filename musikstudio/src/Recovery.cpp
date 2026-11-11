// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Recovery.hpp"

#include "wenuystudio/ProjectIO.hpp"

#include <cstdlib>

#include <filesystem>

#include <stdexcept>

namespace fs=std::filesystem;

namespace wenuystudio {

std::string recoveryPath(const std::string& projectPath){

 if(!projectPath.empty()){

  const auto full=fs::absolute(fs::u8path(projectPath)).lexically_normal();

  if(full.filename().empty())throw std::invalid_argument("Invalid recovery project path");

  return (full.u8string()+".autosave");

 }

 const char* state=std::getenv("XDG_STATE_HOME");

 fs::path base;

 if(state&&*state&&fs::u8path(state).is_absolute())base=fs::u8path(state);

 else {

  const char* home=std::getenv("HOME");

  if(!home||!*home)throw std::runtime_error("HOME or XDG_STATE_HOME is required for unsaved projects");

  base=fs::u8path(home)/".local"/"state";

 }

 return (base/"wenuystudio"/"untitled.wstudio.autosave").u8string();

}

void saveRecovery(const Project& project,const std::string& projectPath){

 const auto p=fs::u8path(recoveryPath(projectPath));

 if(projectPath.empty()){

  fs::create_directories(p.parent_path());

  fs::permissions(p.parent_path(),fs::perms::owner_all,fs::perm_options::replace);

 }

 saveProject(project,p.u8string());

}

bool hasRecovery(const std::string& projectPath) noexcept {

 try{

  const auto recovery=fs::u8path(recoveryPath(projectPath));

  std::error_code ec;

  if(!fs::is_regular_file(recovery,ec)||ec)return false;

  if(projectPath.empty())return true;

  const auto project=fs::u8path(projectPath);

  if(!fs::exists(project,ec))return !ec;

  if(ec)return false;

  const auto recoveryTime=fs::last_write_time(recovery,ec);

  if(ec)return false;

  const auto projectTime=fs::last_write_time(project,ec);

  return !ec&&recoveryTime>=projectTime;

 }catch(...){return false;}

}

Project loadRecovery(const std::string& projectPath){

 if(!hasRecovery(projectPath))throw std::runtime_error("Ingen nyare återställningskopia finns");

 return loadProject(recoveryPath(projectPath));

}

void discardRecovery(const std::string& projectPath){

 const auto path=recoveryPath(projectPath);

 for(const auto& p:{path,path+".bak"}){

  std::error_code ec;

  fs::remove(fs::u8path(p),ec);

  if(ec)throw fs::filesystem_error("Kan inte ta bort återställningskopian",fs::u8path(p),ec);

 }

}

}
