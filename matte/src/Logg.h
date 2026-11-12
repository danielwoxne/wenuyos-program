// Händelseloggen – vad som händer när eleven övar, så att läraren kan se var det skaver.
// En rad JSON per händelse i <datamapp>/logg/<elev>.jsonl. Sparas bara på den här datorn och skickas
// aldrig någonstans. Inga identifierare – bara elevens namn som läraren själv skrivit in.
//
// Händelser (fältet "h"): svar, avbrott, ord, ledtrad, lasupp, pass, paus
#pragma once
#include "Json.h"

#include <string>
#include <vector>

namespace matte {

std::string loggfil(const std::string& elev);
void logga(const std::string& elev, const std::string& handelse, Json data = Json::objekt());   // "t" och "h" läggs till
std::vector<Json> lasLogg(const std::string& elev, int dagar = 0);   // 0 = allt; annars de senaste dagarna
bool rensaLogg(const std::string& elev);
extern size_t LOGG_MAX;                // byte; när filen blir större sparas bara den nyare halvan (2 MB)

}  // namespace matte
