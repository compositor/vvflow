#include <lua.hpp>

#include "lua_tbody.h"
#include "gen_body.h"

int luavvd_gen_cylinder(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_Integer N=0;
    lua_Number  dl=0.0;
    lua_Number  R=0.0;

    N = get_param(L, "N", "positive",
        /*min*/ 1,
        /*max*/ std::numeric_limits<lua_Integer>::max(),
        /*default*/ 0);
    dl = get_param(L, "dl", "positive",
        /*min*/ std::numeric_limits<double>::min(),
        /*max*/ std::numeric_limits<double>::infinity(),
        /*default*/ 0);
    luaL_argcheck(L, (N>0)|(dl>0), 1, "either 'N' or dl' must be specified");
    luaL_argcheck(L, (N>0)^(dl>0), 1, "'N' and 'dl' are mutually exclusive");

    R = get_param(L, "R", "positive", std::numeric_limits<double>::min());

    lua_pushnil(L);
    if (lua_next(L, 1)) {
        const char* param = lua_tostring(L, -2);
        lua_pushfstring(L, "excess parameter '%s'", param);
        luaL_argerror(L, 1, lua_tostring(L, -1));
    }

    std::shared_ptr<TBody> body = std::make_shared<TBody>();
    if (dl>0) {
        gen_arc_dl(body->alist, TVec(0, 0), R, 2*M_PI, 0, dl);
    } else /*if (N>0)*/ {
        gen_arc_N(body->alist, TVec(0, 0), R, 2*M_PI, 0, N);
    }

    body->doUpdateSegments();
    body->doFillProperties();
    pushTBody(L, body);
    return 1;
}

int luavvd_gen_semicyl(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_Integer N = 0;
    lua_Number  dl = 0.0;
    lua_Number  R = 0.0;

    N = get_param(L, "N", "positive",
        /*min*/ 1,
        /*max*/ std::numeric_limits<lua_Integer>::max(),
        /*default*/ 0);
    dl = get_param(L, "dl", "positive",
        /*min*/ std::numeric_limits<double>::min(),
        /*max*/ std::numeric_limits<double>::infinity(),
        /*default*/ 0);
    luaL_argcheck(L, (N>0)|(dl>0), 1, "either 'N' or dl' must be specified");
    luaL_argcheck(L, (N>0)^(dl>0), 1, "'N' and 'dl' are mutually exclusive");

    R = get_param(L, "R", "positive", std::numeric_limits<double>::min());

    lua_pushnil(L);
    if (lua_next(L, 1)) {
        const char* param = lua_tostring(L, -2);
        lua_pushfstring(L, "excess parameter '%s'", param);
        luaL_argerror(L, 1, lua_tostring(L, -1));
    }

    std::shared_ptr<TBody> body = std::make_shared<TBody>();
    if (!dl) {
        double slen = M_PI*R + 2*R;
        dl = slen / N;
    }

    gen_seg_dl(body->alist, TVec(0., 0.), TVec(+R, 0.), dl);
    gen_arc_dl(body->alist, TVec(0, 0), R, 2*M_PI, M_PI, dl);
    gen_seg_dl(body->alist, TVec(-R, 0.), TVec(0., 0.), dl);

    body->doUpdateSegments();
    body->doFillProperties();
    pushTBody(L, body);
    return 1;
}