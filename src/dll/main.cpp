#include "SC4AdvancedLotPlopDirector.hpp"

static SC4AdvancedLotPlopDirector sDirector;

cRZCOMDllDirector* RZGetCOMDllDirector()
{
    static auto sAddedRef = false;
    if (!sAddedRef) {
        sDirector.AddRef();
        sAddedRef = true;
    }
    return &sDirector;
}
