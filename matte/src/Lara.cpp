#include "Lara.h"

namespace matte {

namespace {
std::string id_ = "se.wenuyos.app.matte", miljo_ = "MATTE_DATA";
}

void laraInit(const std::string& appId, const std::string& datamiljo) { id_ = appId; miljo_ = datamiljo; }
const std::string& appId() { return id_; }
const std::string& datamiljo() { return miljo_; }

}  // namespace matte
