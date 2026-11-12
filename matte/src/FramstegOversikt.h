// Matte 1.3 etapp 4 – sammanfattning för eleven. Ren C++17, utan GTK.
#pragma once
namespace matte13 {
struct FramstegOversikt {
    int beharskar = 0;
    int ovar = 0;
    int lar = 0;
    int repetition = 0;
    int stjarnor = 0;
    int besokta = 0;
    double andel = 0.0; // 0.0..1.0, andel registrerade kort som behärskas
};
FramstegOversikt gorFramstegOversikt(int beharskar, int ovar, int lar,
                                     int repetition, int stjarnor);
}
