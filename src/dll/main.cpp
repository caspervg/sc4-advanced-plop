#include "SC4PlopAndPaintDirector.hpp"

static SC4PlopAndPaintDirector sDirector;

cRZCOMDllDirector* RZGetCOMDllDirector()
{
    static auto sAddedRef = false;
    if (!sAddedRef) {
        sDirector.AddRef();
        sAddedRef = true;
    }
    return &sDirector;
}
