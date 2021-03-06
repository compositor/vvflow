#include "./optparse.hpp"

#include "MEpsilonFast.hpp"
#include "MConvectiveFast.hpp"
#include "MDiffusiveFast.hpp"
#include "MStepdata.hpp"
#include "MFlowmove.hpp"
#include "elementary.h"

#include <iostream>
#include <fstream>
#include <cstdio>

#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <ctime>

#include "sensors.cpp"
#include "omp.h"
#define dbg(a) a
//#define dbg(a) cerr << "Doing " << #a << "... " << flush; a; cerr << "done\n";
#define seek(f) while (fgetc(f)!='\n'){}

using std::cerr;
using std::endl;
using std::shared_ptr;

//#define OVERRIDEMOI 0.35

// main options
namespace opt {
    int save_profile = 0;
    int show_progress = 0;

    const char* input;
    const char* sensors_file = NULL;
}

int main(int argc, char** argv)
{
    opt::parse(argc, argv);

    Space S;
    S.load(opt::input);

    // error checking
    #define RAISE(STR) { cerr << "vvflow ERROR: " << STR << endl; return -1; }
    if (S.re <= 0) RAISE("invalid value re<=0");
    #undef RAISE
    bool is_viscous = (S.re != std::numeric_limits<double>::infinity());

    char f_stepdata[256]; snprintf(f_stepdata, 256, "stepdata_%s.h5", S.caption.c_str());
    char f_results[240]; snprintf(f_results, 240, "results_%s", S.caption.c_str());
    char f_sensors_output[256]; snprintf(f_sensors_output, 256, "velocity_%s", S.caption.c_str());
    mkdir(f_results, 0777);

    Stepdata stepdata = {&S, f_stepdata, !!opt::save_profile};

    double dl = S.average_segment_length();
    double min_node_size = dl>0 ? dl*5 : 0;
    double max_node_size = dl>0 ? dl*100 : std::numeric_limits<double>::max();
    TSortedTree tr = {&S, 8, min_node_size, max_node_size};
    MConvectiveFast convective = {&S, &tr};
    MEpsilonFast epsilon = {&S, &tr};
    MDiffusiveFast diffusive = {&S, &tr};
    MFlowmove flowmove = {&S};
    sensors sens(&S, &convective, opt::sensors_file, f_sensors_output);
    #ifdef OVERRIDEMOI
        S.BodyList->at(0)->overrideMoi_c(OVERRIDEMOI);
    #endif

    const void* collision = nullptr;
    while (S.time < S.finish + S.dt/2)
    {
        if (S.BodyList.size())
        {
            dbg(tr.build());

            // решаем слау
            if (collision != nullptr) {
                convective.calc_circulation(&collision);
            }
            convective.calc_circulation(&collision);

            dbg(tr.destroy());
        }

        dbg(flowmove.heat_shed());

        if (S.time.divisibleBy(S.dt_save)  && (double(S.time) >= 0))
        {
            char tmp_filename[256];
            snprintf(tmp_filename, 256, "%s/%%06d.h5", f_results);
            S.save(tmp_filename);
            stepdata.flush();
        }

        dbg(flowmove.vortex_shed());
        dbg(flowmove.streak_shed());

        dbg(S.calc_forces());
        stepdata.write();
        S.zero_forces();

        dbg(tr.build());
        dbg(epsilon.CalcEpsilonFast(/*merge=*/is_viscous));
        dbg(convective.process_all_lists());
        dbg(sens.output());
        if (is_viscous)
        {
            dbg(diffusive.process_vort_list());
            dbg(diffusive.process_heat_list());
        }
        dbg(tr.destroy());

        dbg(flowmove.move_and_clean(true, &collision));
            #ifdef OVERRIDEMOI
                    S.BodyList->at(0)->overrideMoi_c(OVERRIDEMOI);
            #endif
        dbg(flowmove.heat_crop());
        S.time = TTime::add(S.time, S.dt);

        if (opt::show_progress)
            fprintf(stderr, "\rt=%-10g \tN=%-10zd", double(S.time), S.VortexList.size());
    }

    if (opt::show_progress)
        fprintf(stderr, "\n");
}
