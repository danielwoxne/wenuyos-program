// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/MediaCheck.hpp"

#include "wenuystudio/Wave.hpp"

#include <filesystem>

#include <unordered_map>

#include <exception>

#include <cmath>

namespace fs=std::filesystem;

namespace wenuystudio {

namespace {

struct Metadata{MediaIssueKind kind;std::string details;double seconds=0;};

}

const char* mediaIssueName(MediaIssueKind k) noexcept {

 switch(k){case MediaIssueKind::Missing:return "MISSING";case MediaIssueKind::Unreadable:return "UNREADABLE";

 case MediaIssueKind::OutOfRange:return "OUT_OF_RANGE";}

 return "UNKNOWN";

}

std::vector<MediaIssue> inspectMedia(const Project& project){

 std::vector<MediaIssue> issues;

 std::unordered_map<std::string,Metadata> cache;

 for(std::size_t ti=0;ti<project.tracks.size();++ti){

  const auto& track=project.tracks[ti];

  for(std::size_t ci=0;ci<track.clips.size();++ci){

   const auto& clip=track.clips[ci];

   auto it=cache.find(clip.filename);

   if(it==cache.end()){

    Metadata meta{};

    try{

     std::error_code ec;

     if(!fs::is_regular_file(fs::u8path(clip.filename),ec)){

      meta.kind=MediaIssueKind::Missing;

      meta.details=ec?ec.message():"Ljudfilen saknas";

     }else{

      WaveReader reader(clip.filename);

      meta.kind=MediaIssueKind::OutOfRange; // used only if a clip exceeds length

      meta.seconds=static_cast<double>(reader.frames())/reader.rate();

     }

    }catch(const std::exception& e){meta.kind=MediaIssueKind::Unreadable;meta.details=e.what();}

    it=cache.emplace(clip.filename,std::move(meta)).first;

   }

   const Metadata& meta=it->second;

   if(!meta.details.empty() || meta.kind==MediaIssueKind::Missing){

    issues.push_back({ti,ci,meta.kind,clip.filename,meta.details});

   }else if(!std::isfinite(clip.sourceOffsetSeconds)||!std::isfinite(clip.durationSeconds)

    ||clip.sourceOffsetSeconds>=meta.seconds+1e-7

    ||(clip.durationSeconds>0 && clip.sourceOffsetSeconds+clip.durationSeconds>meta.seconds+1e-4)){

    issues.push_back({ti,ci,MediaIssueKind::OutOfRange,clip.filename,"Klippet ligger utanför ljudfilens längd"});

   }

  }

 }

 return issues;

}

}
