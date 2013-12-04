#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <malloc.h>
#include <cstring>
#include <fstream>
#include <time.h>
using namespace std;

#include "space.h"
const char *current_version = "v: 1.3  "; //binary file format

#ifndef DEF_GITINFO
	#define DEF_GITINFO "not available"
#endif
#ifndef DEF_GITDIFF
	#define DEF_GITDIFF "not available"
#endif
static const char* gitInfo = DEF_GITINFO;
static const char* gitDiff = DEF_GITDIFF;
const char* Space::getGitInfo() {return gitInfo;}
const char* Space::getGitDiff() {return gitDiff;}

Space::Space()
{
	VortexList = NULL; //new vector<TObj>();
	HeatList = NULL; //new vector<TObj>();
	BodyList = new vector<TBody*>();

	StreakSourceList = new vector<TObj>();
	StreakList = new vector<TObj>();

	InfSpeedX = new ShellScript();
	InfSpeedY = new ShellScript();
	InfCirculation = 0.;
	gravitation = TVec(0., 0.);
	Finish = DBL_MAX;
	Time = dt = TTime();
	save_dt = streak_dt = profile_dt = TTime(INT32_MAX, 1);
	Re = Pr = 0.;
	InfMarker = TVec(0., 0.);

	name = new char[64];
	name[0] = 0;
}

inline
void Space::FinishStep()
{
	const_for(BodyList, llbody)
	{
		TBody &body = **llbody;
		body.doRotationAndMotion();
	}
	Time= TTime::add(Time, dt);
}

/********************************** SAVE/LOAD *********************************/

void SaveBookmark(FILE* fout, int bookmark, const char *comment)
{
	int64_t tmp = ftell(fout);
	fseek(fout, bookmark*16, SEEK_SET);
	fwrite(&tmp, 8, 1, fout);
	fwrite(comment, 1, 8, fout);
	fseek(fout, tmp, SEEK_SET);
}

void SaveList(vector<TObj> *list, FILE* fout)
{
	int64_t size = list->size_safe();
	fwrite(&size, 8, 1, fout);
	if (!list) return;
	const_for (list, obj)
	{
		fwrite(pointer(obj), 24, 1, fout);
	}
}

void LoadList(vector<TObj> *&list, FILE* fin)
{
	int64_t size; fread(&size, 8, 1, fin);
	TObj obj;
	if (!list) list = new vector<TObj>;
	for (int64_t i=0; i<size; i++)
	{
		fread(&obj, 24, 1, fin);
		list->push_back(obj);
	}
}

void Space::Save(const char* format)
{
	char fname[64]; sprintf(fname, format, int(Time/dt+0.5));
	FILE *fout = fopen(fname, "wb");
	if (!fout) { perror("Error saving the space"); return; }

	fseek(fout, 1024, SEEK_SET);

	SaveBookmark(fout, 0, "Header  ");
	{
		fwrite(current_version, 8, 1, fout);
		fwrite(name, 1, 64, fout);
		fwrite(&Time, 8, 1, fout);
		fwrite(&dt, 8, 1, fout);
		fwrite(&save_dt, 8, 1, fout);
		fwrite(&streak_dt, 8, 1, fout);
		fwrite(&profile_dt, 8, 1, fout);
		fwrite(&Re, 8, 1, fout);
		fwrite(&Pr, 8, 1, fout);
		fwrite(&InfMarker, 16, 1, fout);
		InfSpeedX->write(fout);
		InfSpeedY->write(fout);
		fwrite(&InfCirculation, 8, 1, fout);
		fwrite(&gravitation, 16, 1, fout);
		fwrite(&Finish, 8, 1, fout);
		time_t rt; time(&rt); int64_t rawtime=rt; fwrite(&rawtime, 8, 1, fout);
	}

	//writing lists
	SaveBookmark(fout, 1, "Vortexes"); SaveList(VortexList, fout);
	SaveBookmark(fout, 2, "Heat    "); SaveList(HeatList, fout);
	SaveBookmark(fout, 3, "StrkSrc "); SaveList(StreakSourceList, fout);
	SaveBookmark(fout, 4, "Streak  "); SaveList(StreakList, fout);
	//FIXME implement Attaches

	int bookmark = 4;
	const_for(BodyList, llbody)
	{
		#define body (**llbody)
		// пишем общую информацию о данном теле. 
		SaveBookmark(fout, ++bookmark, "BData   ");
		fwrite(&body.pos, 24, 1, fout);
		fwrite(&body.dPos, 24, 1, fout);
		body.SpeedX->write(fout);
		body.SpeedY->write(fout);
		body.SpeedO->write(fout);
		fwrite(&body.Speed_slae, 24, 1, fout);
		fwrite(&body.Speed_slae_prev, 24, 1, fout);

		fwrite(&body.k, 24, 1, fout);
		fwrite(&body.density, 8, 1, fout);

		fwrite(&body.Force_born, 24, 1, fout);
		fwrite(&body.Force_dead, 24, 1, fout);
		fwrite(&body.Friction_prev, 24, 1, fout);

		SaveBookmark(fout, ++bookmark, "Body    ");
		int64_t size = body.size();
		fwrite(&size, 8, 1, fout);
		const_for (body.List, latt)
		{
			int64_t bc = latt->bc;
			int64_t hc = latt->hc;
			fwrite(pointer(&latt->corner), 16, 1, fout);
			fwrite(pointer(&latt->g), 8, 1, fout);
			fwrite(pointer(&bc), 8, 1, fout);
			fwrite(pointer(&hc), 8, 1, fout);
			fwrite(pointer(&latt->heat_const), 8, 1, fout);
			fwrite(pointer(&latt->gsum), 8, 1, fout);
		}
		#undef body
	}

	fclose(fout);
}

int eq(const char *str1, const char *str2)
{
	for (int i=0; i<8; i++)
	{
		if (str1[i] != str2[i]) return i+1;
	}
	return 9;
}

void Space::Load(const char* fname)
{
	FILE *fin = fopen(fname, "rb");
	if (!fin) { perror("Error loading the space"); return; }

	//loading different lists
	int64_t tmp;
	char comment[9]; comment[8]=0;
	for (int i=0; i<64; i++)
	{
		fseek(fin, i*16, SEEK_SET);
		fread(&tmp, 8, 1, fin);
		fread(comment, 8, 1, fin);
		if (!tmp) continue;
		fseek(fin, tmp, SEEK_SET);

		if (eq(comment, "Header  ")>8)
		{
			char version[8]; fread(&version, 8, 1, fin);
			if (eq(version, current_version) <= 8)
			{
				fprintf(stderr, "Cant read version \"%s\". Current version is \"%s\"\n", version, current_version);
				exit(1);
			}

			fread(name, 1, 64, fin);
			fread(&Time, 8, 1, fin);
			fread(&dt, 8, 1, fin);
			fread(&save_dt, 8, 1, fin);
			fread(&streak_dt, 8, 1, fin);
			fread(&profile_dt, 8, 1, fin);
			fread(&Re, 8, 1, fin);
			fread(&Pr, 8, 1, fin);
			fread(&InfMarker, 16, 1, fin);
			InfSpeedX->read(fin);
			InfSpeedY->read(fin);
			fread(&InfCirculation, 8, 1, fin);
			fread(&gravitation, 16, 1, fin);
			fread(&Finish, 8, 1, fin);

			int64_t rawtime; fread(&rawtime, 8, 1, fin); realtime = rawtime;
		}
		else if (eq(comment, "Vortexes")>8) LoadList(VortexList, fin);
		else if (eq(comment, "Heat    ")>8) LoadList(HeatList, fin);
		else if (eq(comment, "StrkSrc ")>8) LoadList(StreakSourceList, fin);
		else if (eq(comment, "Streak  ")>8) LoadList(StreakList, fin);
		else if (eq(comment, "BData   ")>8)
		{
			TBody *body = new TBody(this);
			BodyList->push_back(body);

			fread(&body->pos, 24, 1, fin);
			fread(&body->dPos, 24, 1, fin);
			body->SpeedX->read(fin);
			body->SpeedY->read(fin);
			body->SpeedO->read(fin);
			fread(&body->Speed_slae, 24, 1, fin);
			fread(&body->Speed_slae_prev, 24, 1, fin);

			fread(&body->k, 24, 1, fin);
			fread(&body->density, 8, 1, fin);

			fread(&body->Force_born, 24, 1, fin);
			fread(&body->Force_dead, 24, 1, fin);
			fread(&body->Friction_prev, 24, 1, fin);
		}
		else if (eq(comment, "Body    ")>8)
		{
			TBody *body = *(BodyList->end()-1);

			TAtt att; att.body = body;
			int64_t size; fread(&size, 8, 1, fin);
			for (int64_t i=0; i<size; i++)
			{
				int64_t bc, hc;
				fread(&att.corner, 16, 1, fin);
				fread(&att.g, 8, 1, fin);
				fread(&bc, 8, 1, fin);
				fread(&hc, 8, 1, fin);
				fread(&att.heat_const, 8, 1, fin);
				att.bc = bc::bc(bc);
				att.hc = hc::hc(hc);
				fread(&att.gsum, 8, 1, fin);

				body->List->push_back(att);
			}
			body->doUpdateSegments();
			body->doFillProperties();
		}
		else fprintf(stderr, "S->Load(): ignoring field \"%s\"", comment);
	}

	EnumerateBodies();

	return;
}

FILE* Space::OpenFile(const char* format)
{
	char fname[64]; sprintf(fname, format, int(Time/dt+0.5));
	FILE *fout;
	fout = fopen(fname, "w");
	if (!fout) { perror("Error opening file"); return NULL; }
	return fout;
}

void Space::CalcForces()
{
	const double C_NyuDt_Pi = dt/(C_PI*Re);
	const double C_Nyu_Pi = 1./(C_PI*Re);
	#define body (**llbody)
	const_for(BodyList, llbody)
	{
		double tmp_gsum = 0;
		//TObj tmp_fric(0,0,0);
		body.Friction_prev = body.Friction;
		body.Friction = TVec3D();

		const_for(body.List, latt)
		{
			tmp_gsum+= latt->gsum;
			latt->Cp += tmp_gsum;
			latt->Fr += latt->fric * C_NyuDt_Pi;
			latt->Nu += latt->hsum * (Re*Pr / latt->dl.abs());

			body.Friction.r -= latt->dl * (latt->fric * C_Nyu_Pi / latt->dl.abs());
			body.Friction.o -= (rotl(latt->r)* latt->dl) * (latt->fric  * C_Nyu_Pi / latt->dl.abs());
			body.Nusselt += latt->hsum * (Re*Pr);
		}

		body.Force_export.r = body.Force_born.r - body.Force_dead.r;
		body.Force_export.o = body.Force_born.o - body.Force_dead.o;
		body.Nusselt /= dt;
	}

	//FIXME calculate total pressure
	#undef body
}

void Space::SaveProfile(const char* fname, TValues vals)
{
	if (!Time.divisibleBy(profile_dt)) return;
	int32_t vals_32=vals, N=0;
	const_for(BodyList, llbody) { N+= (**llbody).size(); }
	if (!N) return;
	if (!VortexList) vals_32 &= ~(val::Cp | val::Fr);
	if (!HeatList) vals_32 &= ~val::Nu;

	FILE *fout = fopen(fname, "ab");
	if (!fout) { perror("Error saving the body profile"); return; }
	if (!ftell(fout)) { fwrite(&vals_32, 4, 1, fout); fwrite(&N, 4, 1, fout); }
	float time_tmp = Time; fwrite(&time_tmp, 4, 1, fout);
	float buf[5];

	const_for(BodyList, llbody)
	{
		const_for((**llbody).List, latt)
		{
			buf[0] = latt->corner.x;
			buf[1] = latt->corner.y;
			buf[2] = latt->Cp/save_dt;
			buf[3] = latt->Fr/save_dt;
			buf[4] = latt->Nu/save_dt;

			fwrite(buf, 4, 2, fout);
			if (vals_32 & val::Cp) fwrite(buf+2, 4, 1, fout);
			if (vals_32 & val::Fr) fwrite(buf+3, 4, 1, fout);
			if (vals_32 & val::Nu) fwrite(buf+4, 4, 1, fout);

			latt->Cp = latt->Fr = latt->Nu = 0;
		}
	}
	fclose(fout);
}

void Space::ZeroForces()
{
	const_for(BodyList, llbody)
	{
		const_for((**llbody).List, latt)
		{
			latt->gsum =
			latt->fric =
			latt->hsum = 0;
			latt->ParticleInHeatLayer = -1;
		}

		(**llbody).Force_dead = TVec3D();
		(**llbody).Force_born = TVec3D();
		(**llbody).Friction = TVec3D();
		(**llbody).Nusselt = 0.;
	}
}

/********************************** SAVE/LOAD *********************************/

int Space::LoadVorticityFromFile(const char* filename)
{
	if ( !VortexList ) VortexList = new vector<TObj>();

	FILE *fin = fopen(filename, "r");
	if (!fin) { cerr << "No file called \'" << filename << "\'\n"; return -1; } 

	TObj obj(0, 0, 0);
	while ( fscanf(fin, "%lf %lf %lf", &obj.r.x, &obj.r.y, &obj.g)==3 )
	{
		VortexList->push_back(obj);
	}

	fclose(fin);
	return 0;
}

int Space::LoadVorticity_bin(const char* filename)
{
	if ( !VortexList ) VortexList = new vector<TObj>();

	fstream fin;
	fin.open(filename, ios::in | ios::binary);
	if (!fin) { cerr << "No file called \'" << filename << "\'\n"; return -1; } 

	fin.seekg (0, ios::end);
	size_t N = (size_t(fin.tellg())-1024)/(sizeof(double)*3);
	fin.seekp(1024, ios::beg);

	TObj obj(0, 0, 0);
	
	while ( fin.good() )
	{
		fin.read(pchar(&obj), 3*sizeof(double));
		VortexList->push_back(obj);
	}

	fin.close();
	return 0;
}

int Space::LoadHeatFromFile(const char* filename)
{
	if ( !HeatList ) HeatList = new vector<TObj>();

	FILE *fin = fopen(filename, "r");
	if (!fin) { cerr << "No file called " << filename << endl; return -1; }

	TObj obj(0, 0, 0);
	while ( fscanf(fin, "%lf %lf %lf", &obj.r.x, &obj.r.y, &obj.g)==3 )
	{
		HeatList->push_back(obj);
	}

	fclose(fin);
	return 0;
}

int Space::LoadStreak(const char* filename)
{
	FILE *fin = fopen(filename, "r");
	if (!fin) { perror("Error opening streak file"); return -1; }

	TObj obj(0, 0, 0);
	while ( fscanf(fin, "%lf %lf %lf", &obj.r.x, &obj.r.y, &obj.g)==3 )
	{
		StreakList->push_back(obj);
	}

	fclose(fin);
	return 0;
}

int Space::LoadStreakSource(const char* filename)
{
	FILE *fin = fopen(filename, "r");
	if (!fin) { perror("Error opening streak source file"); return -1; }

	TObj obj(0, 0, 0);
	while ( fscanf(fin, "%lf %lf %lf", &obj.r.x, &obj.r.y, &obj.g)==3 )
	{
		StreakSourceList->push_back(obj);
	}

	fclose(fin);
	return 0;
}

int Space::LoadBody(const char* filename, int cols)
{
	TBody *body = new TBody(this);
	BodyList->push_back(body);

	FILE *fin = fopen(filename, "r");
	if (!fin) { cerr << "No file called " << filename << endl; return -1; } 

	TAtt att; att.body = body;
	att.heat_const = 0;
	char bc_char('n'), hc_char('n');

	char *pattern;
	switch (cols)
	{
		case 2: pattern = "%lf %lf \n"; break;
		case 3: pattern = "%lf %lf %c \n"; break;
		case 5: pattern = "%lf %lf %c %c %lf \n"; break;
		default: fprintf(stderr, "Bad columns number. Only 2 3 or 5 supported\n"); return -1;
	}

	while ( fscanf(fin, pattern, &att.corner.x, &att.corner.y, &bc_char, &hc_char, &att.heat_const)==cols )
	{
		att.bc = bc::bc(bc_char);
		att.hc = hc::hc(hc_char);
		body->List->push_back(att);
	}

	if (!VortexList)
	const_for(body->List, latt)
	{
		if(latt->bc == bc::noslip) { VortexList = new vector<TObj>(); break; }
	}

	if (!HeatList)
	const_for(body->List, latt)
	{
		if(latt->hc != hc::neglect) { HeatList = new vector<TObj>(); break; }
	}

	fclose(fin);
	body->doUpdateSegments();
	body->doFillProperties();
	EnumerateBodies();

	return 0;
}

void Space::EnumerateBodies()
{
	int eq_no=0;
	TBody *bodyWithInfSteadyBC = NULL;
	//there must be 1 and only 1 body with inf_steady condition

	const_for(BodyList, llbody)
	{
		const_for((**llbody).List, latt)
		{
			if (latt->bc == bc::inf_steady)
			{
				if (bodyWithInfSteadyBC)
					latt->bc = bc::steady;
				else
					bodyWithInfSteadyBC = *llbody;
			}
			latt->eq_no = eq_no;
			eq_no++;
		}

		(**llbody).eq_forces_no = eq_no;
		eq_no+= 3;
	}
}

/********************************* INTEGRALS **********************************/

void Space::ZeroSpeed()
{
	const_for (VortexList, lobj)
	{
		lobj->v = TVec();
	}

	const_for (HeatList, lobj)
	{
		lobj->v = TVec();
	}
}

double Space::integral()
{
	if (!VortexList) return 0;
	double sum = 0;

	const_for (VortexList, obj)
	{
		sum += obj->g * obj->r.abs2();
	}

	return sum;
}

double Space::gsum()
{
	if (!VortexList) return 0;
	double sum = 0;

	const_for (VortexList, obj)
	{
		sum += obj->g;
	}

	return sum;
}

double Space::gmax()
{
	if (!VortexList) return 0;
	double max = 0;

	const_for (VortexList, obj)
	{
		max = ( fabs(obj->g) > fabs(max) ) ? obj->g : max;
	}

	return max;
}

TVec Space::HydroDynamicMomentum()
{
	TVec res(0., 0.);
	if (!VortexList) return res;

	const_for (VortexList, obj)
	{
		res += obj->g * obj->r;
	}
	return res;
}

double Space::AverageSegmentLength()
{
	if (!BodyList) return DBL_MIN;

	double SurfaceLength = BodyList->at(0)->getSurface();
	int N = BodyList->at(0)->size() - 1;

	if (!N) return DBL_MIN;
	return SurfaceLength / N;
}
