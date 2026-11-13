// libwenuy – kalkylblad (grund för kalkylprogram, budgetar, tabeller med formler).
//
//   wenuy::Sheet   glest blad (bara ifyllda celler tar minne), formler med beroenden,
//                  cirkelreferenser upptäcks, CSV in/ut och eget format (.wsheet).
//
// Formler börjar med "=". Stöd:
//   + - * / ^ %, jämförelser (= <> < > <= >=), & (sammanfoga text), parenteser
//   cellreferenser A1, $A$1, områden A1:B10
//   SUM AVERAGE MIN MAX COUNT COUNTA IF AND OR NOT ROUND ABS SQRT POWER LEN UPPER LOWER
//   CONCAT TODAY NOW (svenska namn fungerar också: SUMMA MEDEL ANTAL OM AVRUNDA …)
// Fel visas som i andra kalkylprogram: #DIV/0!  #REF!  #NAME?  #VALUE!  #CIRC!
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace wenuy {

struct CellRef {
    int col = 0, row = 0;   // 0-baserade
    bool operator<(const CellRef& o) const { return row != o.row ? row < o.row : col < o.col; }
    bool operator==(const CellRef& o) const { return col == o.col && row == o.row; }
};

std::string colName(int col);                       // 0 → "A", 26 → "AA"
std::string cellName(CellRef c);                    // {1,2} → "B3"
bool parseCellName(const std::string& s, CellRef* out);

struct Value {
    enum Kind { Empty, Number, Text, Bool, Error } kind = Empty;
    double num = 0;
    std::string text;   // text eller felkod
    static Value number(double d) { Value v; v.kind = Number; v.num = d; return v; }
    static Value str(std::string s) { Value v; v.kind = Text; v.text = std::move(s); return v; }
    static Value boolean(bool b) { Value v; v.kind = Bool; v.num = b; return v; }
    static Value error(const char* e) { Value v; v.kind = Error; v.text = e; return v; }
    std::string display(int decimals = -1) const;   // så som cellen visas
};

class Sheet {
public:
    void set(CellRef c, const std::string& input);   // "12", "Hej", "=A1*2" ("" = töm)
    std::string input(CellRef c) const;              // det användaren skrev
    Value value(CellRef c) const;                    // beräknat värde (cachas)
    std::string display(CellRef c) const { return value(c).display(); }
    bool isFormula(CellRef c) const;

    int usedRows() const;   // största rad + 1
    int usedCols() const;
    const std::map<CellRef, std::string>& cells() const { return cells_; }
    void clear() { cells_.clear(); cache_.clear(); }

    std::string toCsv(char sep = ',') const;         // beräknade värden
    bool fromCsv(const std::string& csv, char sep = 0);   // sep 0 = gissa (, ; tab)
    std::string serialize() const;                   // .wsheet (läsbar text, formler bevaras)
    bool deserialize(const std::string& data);

    // Infoga/ta bort rader och kolumner – formler som pekar förbi justeras
    void insertRows(int at, int n);
    void insertCols(int at, int n);
    void removeRows(int at, int n);
    void removeCols(int at, int n);

private:
    friend struct Evaluator;
    void shift(bool rows, int at, int n);
    std::map<CellRef, std::string> cells_;
    mutable std::map<CellRef, Value> cache_;
    mutable std::vector<CellRef> evalStack_;
};

}  // namespace wenuy
