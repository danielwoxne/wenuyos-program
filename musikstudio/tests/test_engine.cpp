// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Engine.hpp"

#include <cassert>

#include <cmath>

#include <cstdio>

#include <iostream>

#include <stdexcept>

void require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}

int main(){

    try{

        const std::string a="wenuy_test_a.wav",b="wenuy_test_b.wav";

        wenuystudio::Project p;

        p.sampleRate=48000;p.durationSeconds=0.5;

        wenuystudio::Track instrument;instrument.notes.push_back({69,0,.3,.9f});

        p.tracks.push_back(instrument);

        wenuystudio::render(p,a);

        wenuystudio::WaveReader first(a);

        require(first.rate()==48000,"Sample rate mismatch");

        require(first.frames()==24000,"Frame count mismatch");

        wenuystudio::Stereo frames[512];first.read(0,frames,512);

        bool audible=false;

        for(const auto& v:frames){require(std::isfinite(v.l),"Nonfinite audio");if(std::abs(v.l)>0.0001f)audible=true;}

        require(audible,"Synth output silent");

        wenuystudio::Project p2;p2.sampleRate=48000;p2.durationSeconds=.5;

        wenuystudio::Track clip;clip.clips.push_back({a,0.0,0.0,0.0,1.0f});

        clip.pan=-1.0f;p2.tracks.push_back(clip);

        wenuystudio::render(p2,b);

        wenuystudio::WaveReader second(b);second.read(0,frames,512);

        bool leftOnly=false;

        for(const auto& f:frames){if(std::abs(f.l)>0.001f)leftOnly=true;require(std::abs(f.r)<.0001f,"Panned output leaked right");}

        require(leftOnly,"Panned audio silent");

        bool bad=false;

        try{wenuystudio::Project x=p;x.sampleRate=22050;x.tracks.clear();

             wenuystudio::Track t;t.clips.push_back({a,0,0,0,1});x.tracks.push_back(t);wenuystudio::render(x,b);}

        catch(const std::runtime_error&){bad=true;}

        require(!bad,"Sample-rate conversion failed");

        std::remove(a.c_str());std::remove(b.c_str());

        std::cout<<"All offline audio engine tests passed\n";

        return 0;

    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}

}
