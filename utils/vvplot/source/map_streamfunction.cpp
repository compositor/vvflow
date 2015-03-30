#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <math.h>
#include <time.h>

#include "libvvplot_api.h"
#include "core.h"
#include "hdf5.h"

static double Rd2;
static TVec RefFrame_Speed;
inline static double atan(TVec p) {return atan(p.y/p.x);}

double Psi(Space* S, TVec p)
{
	double psi1=0.0, psi2=0.0, psi3=0.0;

	for (auto& lbody: S->BodyList)
	{
		double psi_g_tmp=0, psi_q_tmp=0;

		if (!lbody->speed_slae.iszero())
		for (TAtt& latt: lbody->alist)
		{
			TVec Vs = lbody->speed_slae.r + lbody->speed_slae.o * rotl(latt.r - lbody->get_axis());
			double g = -Vs * latt.dl;
			double q = -rotl(Vs) * latt.dl;
			psi_g_tmp+= log((p-latt.r).abs2() + Rd2) * g;
			psi_q_tmp+= atan(p-latt.r) * q;
		}

		for (auto& latt: lbody->alist)
		{
			psi2 += log((p-latt.r).abs2() + Rd2) * latt.g;
		}
	}
	psi2*= -0.5*C_1_2PI;

	TSortedNode* Node = S->Tree->findNode(p);
	if (!Node) return 0;
	for (TSortedNode* lfnode: *Node->FarNodes)
	{
		TObj obj = lfnode->CMp;
		psi3+= log((p-obj.r).abs2() + Rd2)*obj.g;
		obj = lfnode->CMm;
		psi3+= log((p-obj.r).abs2() + Rd2)*obj.g;
	}
	for (TSortedNode* lnnode: *Node->NearNodes)
	{
		for (TObj *lobj = lnnode->vRange.first; lobj < lnnode->vRange.last; lobj++)
		{
			psi3+= log((p-lobj->r).abs2() + Rd2) * lobj->g;
		}
	}
	psi3*= -0.5*C_1_2PI;

	return psi1 + psi2 + psi3 + p*rotl(S->InfSpeed() - RefFrame_Speed);
}

extern "C" {
int map_streamfunction(hid_t fid, char RefFrame, double xmin, double xmax, double ymin, double ymax, double spacing)
{
	Space *S = new Space();
	S->Load(fid);

	double dl = S->AverageSegmentLength(); Rd2 = dl*dl/25;
	S->Tree = new TSortedTree(S, 8, dl*20, 0.3);

	/**************************** LOAD ARGUMENTS ******************************/
	switch (RefFrame)
	{
		case 'o': RefFrame_Speed = TVec(0, 0); break;
		case 'f': RefFrame_Speed = S->InfSpeed(); break;
		case 'b': RefFrame_Speed = S->BodyList[0]->speed_slae.r; break;
		default:
		fprintf(stderr, "Bad reference frame\n");
		fprintf(stderr, "Available options are:\n");
		fprintf(stderr, " 'o' : original reference frame\n" );
		fprintf(stderr, " 'f' : fluid reference frame\n" );
		fprintf(stderr, " 'b' : body reference frame\n" );
	}


	S->Tree->build();
	hsize_t dims[2] = {
		static_cast<size_t>((xmax-xmin)/spacing) + 1,
		static_cast<size_t>((ymax-ymin)/spacing) + 1
	};
	float *mem = (float*)malloc(sizeof(float)*dims[0]*dims[1]);

	for (size_t xi=0; xi<dims[0]; xi++)
	{
		double x = xmin + double(xi)*spacing;
		#pragma omp parallel for ordered schedule(dynamic, 10)
		for (size_t yj=0; yj<dims[1]; yj++)
		{
			double y = ymin + double(yj)*spacing;
			mem[xi*dims[1]+yj] = Psi(S, TVec(x, y));
		}
	}

	char map_name[] = "map_streamfunction.?";
	map_name[19] = RefFrame;
	map_save(fid, map_name, mem, dims, xmin, xmax, ymin, ymax, spacing);
	free(mem);

	return 0;
}}
