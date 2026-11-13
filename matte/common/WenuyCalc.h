// WenuyOS räknare – uttryck och enhetsomvandling. Används av Kalkylatorn och sökfältet.
//
//   wenuy::calc::evaluate("2^10 + sqrt(16)", v)          → 1028
//   wenuy::calc::evaluate("sin(30)", v, nullptr, true)   → 0,5 (grader)
//   wenuy::calc::convert("5 km i mil", s)                → "5 km = 0,5 mil"
//   wenuy::calc::convert("100 f to c", s)                → "100 °F = 37,78 °C"
#pragma once
#include <string>
#include <vector>

namespace wenuy::calc {
// + − × ÷ ^ % ! () , decimaler med punkt eller komma, tal som 1e3, mellanslag i tal (1 000),
// funktioner sqrt cbrt sin cos tan asin acos atan sinh cosh tanh ln log log2 exp abs round floor ceil,
// konstanter pi π e, "ans" = förra svaret. "50 %" = 0,5; "200 + 10 %" = 220.
bool evaluate(const std::string& expr, double& out, std::string* error = nullptr, bool degrees = false, double ans = 0);
std::string format(double v, bool swedish, int digits = 12);

struct Unit { const char* id; const char* en; const char* sv; const char* category; };
const std::vector<Unit>& units();
std::vector<std::string> categories();                         // "length", "mass", …
const char* categoryName(const std::string& cat, bool swedish);
bool convertValue(double v, const std::string& from, const std::string& to, double& out);
// "5 km i mil", "5km to miles", "20 c i f", "3 GB i MB" → "5 km = 0,5 mil"
bool convert(const std::string& query, std::string& out, bool swedish = true);
}  // namespace wenuy::calc
