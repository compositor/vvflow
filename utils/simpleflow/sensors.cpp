#include "stdio.h"

#include "core.h"
#include "convectivefast.h"

class sensors
{
	public:
		sensors(Space* sS, const char* sensors_file, const char* output);
		void loadFile(const char* file);
		void output();

	private:
		Space *S;
		FILE *fout;
		vector <TVec> *slist;
};

sensors::sensors(Space* sS, const char* sensors_file, const char* output)
{
	S = sS;
	fout = NULL;
	slist = NULL;

	loadFile(sensors_file);
	if (slist->size_safe()) fout = fopen(output, "a");
}

void sensors::loadFile(const char* file)
{
	if (!file) return;

	FILE *fin = fopen(file, "r");
	if (!fin) { cerr << "No file called \'" << file << "\'\n"; return; }

	slist = new vector<TVec>();
	TVec vec(0, 0);
	while ( fscanf(fin, "%lf %lf", &vec.rx, &vec.ry)==2 )
	{
		slist->push_back(vec);
	}

	fclose(fin);
}

void sensors::output()
{
	if (!fout) return;

	fprintf(fout, "%lg", S->Time);
	const_for(slist, lvec)
	{
		TVec tmp = SpeedSumFast(*lvec);
		fprintf(fout, " \t%lg \t%lg", tmp.rx, tmp.ry);
	}
	fprintf(fout, "\n");
	fflush(fout);
}
