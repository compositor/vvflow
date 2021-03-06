#include "XVorticity.hpp"

#include "MFlowmove.hpp"
#include "MEpsilonFast.hpp"
#include "elementary.h"

#include <cmath>
#include <limits>

using std::max;
using std::exp;
using std::vector;

XVorticity::XVorticity(
    const Space &S,
    double xmin, double ymin,
    double dxdy,
    int xres, int yres
):
    XField(xmin, ymin, dxdy, xres, yres),
    eps_mult(),
    S(S),
    dl(S.average_segment_length())
{}

void XVorticity::evaluate()
{
    if (eps_mult <= 0)
        throw std::invalid_argument("XVorticity(): eps_mult must be positive");

    if (evaluated)
        return;

    S.HeatList.clear();
    S.StreakList.clear();
    S.StreakSourceList.clear();
    MFlowmove flowmove = {&S};
    flowmove.vortex_shed();

    TSortedTree tree(&S, 8, dl*20);
    tree.build();

    const vector<TSortedNode*>& bnodes = tree.getBottomNodes();

    #pragma omp parallel
    {
        #pragma omp for
        for (auto llbnode = bnodes.cbegin(); llbnode < bnodes.cend(); llbnode++)
        {
            for (TObj *lobj = (**llbnode).vRange.first; lobj < (**llbnode).vRange.last; lobj++)
            {
                lobj->v.x = 1./(sqr(eps_mult)*std::max(MEpsilonFast::eps2h(**llbnode, lobj->r), sqr(0.6*dl)));
                lobj->v.y = lobj->v.x * lobj->g;
            }
        }

        #pragma omp barrier
        // Calculate field ********************************************************

        #pragma omp for collapse(2) schedule(dynamic, 256)
        for (int yj=0; yj<yres; yj++)
        {
            for (int xi=0; xi<xres; xi++)
            {
                TVec p = TVec(xmin, ymin) + dxdy*TVec(xi, yj);
                const TSortedNode* bnode = tree.findNode(p);
                map[yj*xres+xi] = S.point_is_in_body(p) ? 0 : vorticity(*bnode, p);
            }
        }
    }

    evaluated = true;
}

double XVorticity::vorticity(const TSortedNode &node, TVec p) const
{
    double res = 0;

    for (const TSortedNode* lnnode: *node.NearNodes)
    {
        for (const TObj *lobj = lnnode->vRange.first; lobj < lnnode->vRange.last; lobj++)
        {
            double exparg = -(p-lobj->r).abs2() * lobj->v.x; // v.x stores eps^(-2)
            res+= (exparg>-6) ? lobj->v.y * exp(exparg) : 0; // v.y stores g*eps(-2)
        }
    }

    res *= C_1_PI;

    double erfarg = MEpsilonFast::h2(node, p)/sqr(dl*eps_mult);
    res += (erfarg<3) ? 0.5*(1-erf(erfarg)) : 0;

    return res;
}
