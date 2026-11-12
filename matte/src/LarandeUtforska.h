#pragma once
#include <string>

// Matematiska värden skilda från GTK och elevens rättning.
namespace matte13 {
struct Tallinje {
    double min = 0.0, max = 20.0, steg = 1.0, markerat = 5.0;
};
bool giltig(Tallinje l);
double snappa(Tallinje l, double tal);
double andel(Tallinje l, double tal);
std::string svTal(double v);
bool hittaTallinje(const std::string& lektion, const std::string& fraga, Tallinje& ut);

enum class Geometriform { Rektangel, RatvinkligTriangel };
struct Geometri {
    Geometriform form = Geometriform::Rektangel;
    double bredd = 4, hojd = 3;
};
bool giltig(Geometri g);
double area(Geometri g);
double omkrets(Geometri g);
bool arGeometrilektion(const std::string& lektion);
} // namespace matte13
