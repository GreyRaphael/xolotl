#include <string>
#include <map>
#include "Species.h"

namespace xolotlCore {

std::string toString(const Species& s) {

    static std::map<Species, std::string> smap {
        { Species::Invalid, "Invalid_species" },
        { Species::V, "V" },
        { Species::I, "I" },
        { Species::He, "He" },
        { Species::Xe, "Xe" },
    };

    auto iter = smap.find(s);
    return (iter != smap.end()) ? iter->second : "[unrecognized species]";
}


} // namespace xolotlCore

