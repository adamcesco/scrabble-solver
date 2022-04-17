#ifndef SCRABBLE_SOLVER_RMAC_H
#define SCRABBLE_SOLVER_RMAC_H

#include "../CADS/CADS.h"

namespace scl{
    struct RMAC {    //Rack Map And Calculator
        RMAC() = default;

        RMAC(const std::string &, const std::string &);

        RMAC(const std::string &, const std::unordered_set<std::string> &);

        bool is_descendent_of(scl::Tstring &, const int *, int) const;

        std::string rack;
        std::vector<scl::Tstring> data;
    };
}


#endif //SCRABBLE_SOLVER_RMAC_H
