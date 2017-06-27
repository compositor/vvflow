#ifndef BODY_H_
#define BODY_H_

class Space;
class TBody;
class TAtt;

#include "elementary.h"
#include <vector>
#include <memory>

using std::vector;
using std::weak_ptr;

//boundary condition
enum class bc_t { steady, kutta };
enum class hc_t { neglect, isolate, const_t, const_w };

class TAtt : public TObj
{
    public:
        //rx, ry: center coordinates ; $\vec r$ in doc
        //g: unkonown circulation $\gamma$ in doc
        TVec corner; //$\vec c$ in doc
        // v variable isnt used
        //_1_eps always stores 3.0/dl.abs()
        //double gatt, qatt; //$q_\att, \gamma_\att$ in doc
        int64_t  heat_layer_obj_no;
        float    heat_const;
        uint32_t slip;

        TVec dl; //$\Delta \vec l$ in doc
        double gsum; //filled by flowmove: MoveAndClean() & VortexShed()
        double hsum; //filled by flowmove: MoveAndClean() & HeatShed()
        double fric; //filled by diffusivefast: SegmentInfluence()
        double Cp; // computed by S->CalcForces;
        double Fr; // computed by S->CalcForces;
        double Nu; // computed by S->CalcForces;

        TAtt():TObj(), heat_layer_obj_no(-1), heat_const(0.0), slip(0)
        {
            gsum = hsum = fric = 0.0;
            Cp = Fr = Nu = 0.0;
        }
        //TAtt(TBody *body, int eq_no);
        //void zero() { r.x = r.y = g = gsum = hsum = /*FIXME fric?*/ Cp = Fr = Nu = 0; heat_layer_obj_no = -1; }

    public:
        // TBody* body;
        uint32_t eq_no;
};

class TBody
{
    public:
        TBody();
        ~TBody() = default;
        TBody(const TBody&) = delete;
        TBody(TBody&&) = delete;
        TBody& operator= (const TBody&) = delete;
        TBody& operator= (TBody&&) = delete;

        int load_txt(const char* filename);
        // int load_bin(const char* filename);

        std::string label;
        // FIXME rename to lower case
        std::vector<TAtt> alist;
        weak_ptr<TBody> root_body;

        TVec3D holder; //in doc = $R_bx$, $R_by$, $\alpha_b$
        TVec3D dpos; // same with delta
        TVec3D kspring; // spring const: kx, ky, ka;
        TVec3D damping; // spring damping: tau_x, tau_y, tau_a;
        TVec3D speed_slae;
        TVec3D speed_slae_prev;

        bool collision_detected;
        // coordinates of collision
        TVec3D collision_min;
        TVec3D collision_max;
        double bounce;

        //double kx, ky, ka;
        double density; //in doc rho_b/rho_0
        int32_t special_segment_no;
        bc_t boundary_condition;
        hc_t heat_condition;

        TVec3D friction, friction_prev; //computed by S->CalcForces
        TVec3D force_hydro, force_holder; //computed by convectivefast->CalcCirculationFast
        double nusselt; //computed by S->CalcForces
        TVec3D fdt_dead; //сила, индуцированная умирающими вихрями
        // перед использованием силу надо поделить на dt, а момент - на 2*dt
        double g_dead;

    public:
        //functions \vec V(t), \omega(t)
        ShellScript speed_x;
        ShellScript speed_y;
        ShellScript speed_o;
        TVec3D speed(double t) const;

        //see \vec c_s \vert_Rotation
        void move(TVec3D deltaHolder, TVec3D deltaBody);
        //update TAtt-> rx, ry, dl after doRotationAndMotion()
        void doUpdateSegments();

        TAtt* isPointInvalid(TVec p);
        TAtt* isPointInHeatLayer(TVec p);
        bool isInsideValid() {return _area<=0;}

        void doFillProperties();
        double get_surface() const {return _surface;}
        double get_area() const {return _area;}
        TVec   get_com() const {return _com;} // center of mass
        TVec   get_axis() const {return holder.r + dpos.r;}
        double get_moi_c() const {return _moi_c;} // moment of inertia about rotation axis
        void override_moi_c(double newMoi_c) {_moi_c = newMoi_c;}
        size_t size() const    {return alist.size();}

        int eq_forces_no; // starting number of forces equations

        //Heat layer
        //void CleanHeatLayer();
        //int *ObjectIsInHeatLayer(TObj &obj); //link to element of HeatLayer array

    private:
        double _surface;
        double _area;
        TVec   _com; //center of mass (in global ref frame)
        double _moi_com; //moment of inertia about com;
        double _moi_c; //moi about rotation axis
        double _min_disc_r2; //min((corner-com).abs2())
        TVec   _min_rect_bl; //TVec(min(corner.x), min(corner.y))
        TVec   _min_rect_tr; //TVec(max(corner.x), max(corner.y))

        std::vector<TVec> heat_layer;
        template <class T>
            TAtt* isPointInContour(TVec p, vector<T> &list);
};

#endif /* BODY_H_ */
