// SPDX-License-Identifier: GPL-3.0-or-later

#include "wenuystudio/Engine.hpp"

#include <exception>

#include <iostream>

int main(int argc,char** argv){

    if(argc!=2){std::cerr<<"Usage: wenuy-studio-render OUTPUT.wav\nCreates a 4-note audio engine test (not the finished studio UI).\n";return 2;}

    try {

        wenuystudio::Project p;

        p.sampleRate=48000;p.durationSeconds=3.0;

        wenuystudio::Track track;

        track.name="Piano proof of concept";

        track.notes={{60,0.0,0.6,.70f},{64,.6,.6,.70f},{67,1.2,.6,.70f},{72,1.8,1.0,.70f}};

        p.tracks.push_back(track);

        wenuystudio::render(p,argv[1]);

        std::cout<<"Created "<<argv[1]<<"\n";

        return 0;

    }catch(const std::exception& e){std::cerr<<"Wenuy Studio render error: "<<e.what()<<"\n";return 1;}

}
