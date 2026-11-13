// SPDX-License-Identifier: GPL-3.0-or-later
#include "wenuystudio/InstrumentBank.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace wenuystudio {
namespace {
constexpr double pi=3.14159265358979323846;
constexpr std::array<InstrumentPreset,16> presets{{
 {0,"Sine","Sinus","Synthesizers","Syntar"},
 {1,"Triangle","Triangel","Synthesizers","Syntar"},
 {2,"Organ","Orgel","Keyboards","Tangentinstrument"},
 {3,"Warm lead","Varm lead","Synthesizers","Syntar"},
 {4,"Piano (synth)","Piano (syntes)","Keyboards","Tangentinstrument"},
 {5,"Electric piano","Elpiano","Keyboards","Tangentinstrument"},
 {6,"Music box","Speldosa","Percussion","Slagverk"},
 {7,"Plucked guitar","Plockad gitarr","Strings","Stränginstrument"},
 {8,"Electric bass","Elbas","Strings","Stränginstrument"},
 {9,"String ensemble","Stråkensemble","Strings","Stränginstrument"},
 {10,"Cello (synth)","Cello (syntes)","Strings","Stränginstrument"},
 {11,"Flute","Flöjt","Winds","Blåsinstrument"},
 {12,"Clarinet","Klarinett","Winds","Blåsinstrument"},
 {13,"Brass","Bleckblås","Winds","Blåsinstrument"},
 {14,"Choir pad","Körmatta","Voices","Röster"},
 {15,"Marimba","Marimba","Percussion","Slagverk"}
}};
float partial(double phase,double hz,double multiplier,double nyquist) noexcept {
 return hz*multiplier<nyquist?static_cast<float>(std::sin(phase*multiplier)):0.f;
}
} // namespace

std::size_t instrumentCount() noexcept {return presets.size();}
const InstrumentPreset* instrumentById(int id) noexcept {
 return id>=0&&static_cast<std::size_t>(id)<presets.size()?
  &presets[static_cast<std::size_t>(id)]:nullptr;
}

float instrumentSample(int id,double phase,double hz,double t,double duration,
                       std::uint32_t rate) noexcept {
 if(!instrumentById(id)||!std::isfinite(phase)||!std::isfinite(hz)||
    !std::isfinite(t)||!std::isfinite(duration)||hz<=0||rate<8000||
    t<0||duration<=0||t>=duration)return 0.f;
 const double limit=.48*rate;
 // Compute only harmonics used by this preset. Pure sine and triangle notes
 // must not pay for two extra sin() calls on every sample on 2 GB CPUs.
 const float fundamental=partial(phase,hz,1,limit);
 const auto second=[&]() noexcept {return partial(phase,hz,2,limit);};
 const auto third=[&]() noexcept {return partial(phase,hz,3,limit);};
 float wave=0;
 double attack=.006,release=.09,decay=0;
 switch(id) {
  case 0: wave=fundamental;break;
  case 1: wave=static_cast<float>((2/pi)*std::asin(std::clamp(
                  static_cast<double>(fundamental),-1.0,1.0)));break;
  case 2: wave=.70f*fundamental+.25f*second()+.15f*third();break;
  case 3: wave=.65f*fundamental+
      .20f*partial(phase,hz,1.005,limit)+.12f*second();break;
  case 4: { // Piano: bright attack, harmonics damp faster than fundamental.
           const float bright=static_cast<float>(1./(1.+9.*t));
           wave=.77f*fundamental+bright*(.18f*second()+.08f*third());
           attack=.003;release=.20;decay=1.1;break;
          }
  case 5: { // Electric piano: metallic transient that quickly softens.
           const float bright=static_cast<float>(1./(1.+14.*t));
           wave=.75f*partial(phase+.55*std::sin(phase*2),hz,1,limit)
                 +.12f*second()*bright;
           attack=.003;release=.16;decay=1.6;break;
          }
  case 6: wave=.70f*fundamental+.20f*partial(phase,hz,2.76,limit);
          attack=.002;release=.25;decay=.7;break;
  case 7: { // A picked string loses brightness as it rings.
           const float bright=static_cast<float>(1./(1.+12.*t));
           wave=.72f*fundamental+bright*(.23f*second()+.13f*third());
           attack=.002;release=.14;decay=.55;break;
          }
  case 8: { // Bass fundamental survives beyond the short attack transient.
           const float bright=static_cast<float>(1./(1.+10.*t));
           wave=.87f*fundamental+.13f*second()*bright;
           attack=.002;release=.08;decay=1.3;break;
          }
  case 9: wave=.63f*fundamental+.22f*second()+.12f*third();
          attack=.14;release=.22;break;
  case 10: wave=.73f*fundamental+.19f*second()-.08f*third();
           attack=.09;release=.25;break;
  case 11: wave=.94f*fundamental+.05f*second();
           attack=.035;release=.12;break;
  case 12: wave=.68f*fundamental+.24f*third()+
            .06f*partial(phase,hz,5,limit);
           attack=.03;release=.11;break;
  case 13: wave=.50f*fundamental+.27f*second()+.16f*third()+
            .05f*partial(phase,hz,4,limit);
           attack=.018;release=.15;break;
  case 14: wave=.67f*fundamental+.17f*second()+
            .14f*partial(phase,hz,1.003,limit);
           attack=.20;release=.28;break;
  case 15: { // Marimba's inharmonic strike fades ahead of its body tone.
            const float bright=static_cast<float>(1./(1.+16.*t));
            wave=.73f*fundamental+.19f*bright*partial(phase,hz,3.95,limit);
            attack=.002;release=.16;decay=.52;break;
           }
 }
 // Fast attack/smooth release per preset; short notes stay bounded.
 const double gate=std::clamp(std::min(t/attack,(duration-t)/release),0.0,1.0);
 const double tail=decay>0?std::exp(-t/decay):1.0;
 return static_cast<float>(std::clamp(static_cast<double>(wave)*gate*tail,-1.0,1.0));
}
}
