/* 
voicing_analyzer:


*/
#include <stdlib.h>
#include <math.h>
#include <time.h>
// for string manipulation
#include <string.h>
#include <ctype.h>
#include "m_pd.h"

// to sort arrays
#include "sglib.h"


#define VOICES 5

#define NOTES_RANGE 80 // this should be multiple of 16
#define LOWER_POSSIBLE_NOTE 24 // lower note possible, it should be a C
#define POSSIBLE_NOTES (NOTES_RANGE/12*4) // 4 is the max number of notes in a chord

// default values 
#define DEF_WIDENESS 3 // 3 octaves
#define DEF_CENTER_NOTE 72 // central C

#define DEBUG 0 // messaggi di debug
#define DEBUG_VERBOSE 0 // messaggi di debug

static t_class *voicing_analyzer_class;


typedef struct _voicing_analyzer
{
    t_object x_obj; // myself
	int current_voices[VOICES];
	int previous_voices[VOICES];
	t_outlet *small_intervals_out, *i_like_parallelism_out,
		*center_note_out, *wideness_out;
	
} t_voicing_analyzer;


void voicing_analyzer_free(t_voicing_analyzer *x)
{
//	freebytes(x->buf_strum1, sizeof(x->buf_strum1));
//	freebytes(x->buf_strum2, sizeof(x->buf_strum2));
}


// here i evaluate this voicing
void analyze_it(t_voicing_analyzer *x, float *wideness, float *i_like_parallelism, int *center_note, float *small_intervals)
{
	int i, j, tmp, res, last, avgHI, avgLOW, sameDirection, parallel8_5;
	int min,max, distance;
	short int chord_notes[4];
	short int chord_notes_ok[4];
	short int transitions[VOICES];
	short int directions[VOICES];
	// intervals between voices
	// for parallel and hidden 5ths
	// voices spacing etc..
	short int intervals[VOICES][VOICES]; 
	short int notes[VOICES];
	res = 0; // starting fitness
	tmp = 0;

 	// shared objects
	for (i=0; i<VOICES; i++)
	{
		notes[i]=x->current_voices[i];
		transitions[i] = x->current_voices[i] - x->previous_voices[i];
		if (transitions[i]!=0)
			directions[i] = transitions[i]/abs(transitions[i]);
		else
			directions[i] = 0;
		if (DEBUG_VERBOSE)
			post("directions[%i]=%i", i, directions[i]);

	}
	for (i=0; i<VOICES; i++)
	{
		for (j=i+1; j<VOICES; j++)
		{
			intervals[i][j] = (x->current_voices[i] - x->current_voices[j])%12 ;
			if (DEBUG_VERBOSE)
				post("intervals[%i][%i]=%i", i, j, intervals[i][j]);
		}
	}
	SGLIB_ARRAY_SINGLE_QUICK_SORT(short int, notes, VOICES, SGLIB_NUMERIC_COMPARATOR)

	sameDirection = 0; 
	parallel8_5 = 0;
	
	// all same direction? 
	if ( directions[0]==directions[1] &&
	directions[1]==directions[2] &&
	directions[2]==directions[3])
	{
		sameDirection=1;
		if (DEBUG_VERBOSE)
			post("same direction!");
	}
	
	// parallel 5ths or octaves? (if yes return 0)
	// how?
	// hidden 8ths nor 5ths ?
	for (i=0; i<VOICES; i++)
	{
		for (j=i+1; j<VOICES; j++)
		{
			if (intervals[i][j]==7 || intervals[i][j]==0)
			{
				// hidden or parallel 5th,octave or unison
				// bad!
				if (directions[i]==directions[j])
				{
					parallel8_5 += 1;
					if (DEBUG_VERBOSE)
						post("hidden or parallel consonance!");
				}
			}
		}
	}

	// now i can compute parallelism
	*i_like_parallelism = (float) -1;
	*i_like_parallelism += sameDirection;
	*i_like_parallelism += (float) ( ((float)(VOICES*(VOICES-1))) / ((float)parallel8_5)  );

	// is voice spacing uniform ?(except for the bass)
	// TODO: use notes[]
	// are voices average centered?
	for (i=0; i<VOICES; i++)
	{
		tmp+=notes[i];
		if (DEBUG_VERBOSE)
			post("average note is %i at passage %i", tmp, i);
	}
	// this is the average note
	*center_note = tmp/(VOICES);

	// are intervals small?
	tmp=0;
	for (i=0; i<VOICES; i++)
	{
		if (DEBUG_VERBOSE)
			post("transitions[%i] = %i",i, transitions[i]);
		res-=abs(transitions[i]);
		// give an incentive for semitones etc..
		if (transitions[i]==0)
			res += 5;
		if (abs(transitions[i]==1))
			res += 5;
		if (abs(transitions[i]==2))
			res += 5;
		if (abs(transitions[i]==3))
			res += 2;
		if (abs(transitions[i]==4))
			res += 2;
		if (abs(transitions[i]==5))
			res += 1;
		if (abs(transitions[i]==6))
			res += 1;
		if (abs(transitions[i]>11))
			res -= 2;
		if (abs(transitions[i]>15))
			res -= 3;
	}

	*small_intervals = (float) (((float) res) / ((float) (5 * VOICES)));

	// now wideness	
	min = notes[0];
	max = notes[VOICES-1];
	distance = max - min;
	*wideness = (float) (((float)distance) / ((float)12));




}

typedef struct fitness_list_element_t 
{
	int index;
	int fitness;
} fitness_list_element;

#define FITNESS_LIST_COMPARATOR(e1, e2) (e1.fitness - e2.fitness)

void analyze_voicing(t_voicing_analyzer *x)
{
	t_atom lista[VOICES];
	float small_intervals=0;
	float i_like_parallelism=0;
	float wideness=0;
	int center_note=0;

	analyze_it(x, &wideness, &i_like_parallelism, &center_note, &small_intervals);
	
	// order is important!
	outlet_float(x->i_like_parallelism_out, i_like_parallelism);
	outlet_float(x->small_intervals_out, small_intervals);
	outlet_float(x->wideness_out, wideness);
	outlet_float(x->center_note_out, center_note);
	
}

// called when i send a list (with midi values)
void set_current_voices(t_voicing_analyzer *x, t_symbol *sl, int argc, t_atom *argv)
{
	int i=0;	
	
	if (argc<VOICES)
	{
		error("insufficient notes sent!");
		return;
	}
	// fill input array with actual data sent to inlet
	for (i=0;i<VOICES;i++)
	{
		x->previous_voices[i] = x->current_voices[i];
		x->current_voices[i] = atom_getint(argv++);
	}

	analyze_voicing(x);


}


void print_help(t_voicing_analyzer *x)
{
	post("");
	post("voicing_analyzer is an external that analyze voicing");
	post("takes as input %i midi notes", VOICES);
	post("available commands:");
	post("this externalis part of the frank framework");
	post("authors: davide morelli, david casals");

}

void *voicing_analyzer_new(t_symbol *s, int argc, t_atom *argv)
{
	int i;
	time_t a;
    t_voicing_analyzer *x = (t_voicing_analyzer *)pd_new(voicing_analyzer_class);
	x->center_note_out = outlet_new(&x->x_obj, gensym("float"));
	x->wideness_out = outlet_new(&x->x_obj, gensym("float"));
	x->small_intervals_out = outlet_new(&x->x_obj, gensym("float"));
	x->i_like_parallelism_out = outlet_new(&x->x_obj, gensym("float"));	

	for (i=0;i<VOICES;i++)
	{
		x->previous_voices[i] = 0;
		x->current_voices[i] = 0;
	}

	return (x);
}

void voicing_analyzer_setup(void)
{
    voicing_analyzer_class = class_new(gensym("voicing_analyzer"), (t_newmethod)voicing_analyzer_new,
        (t_method)voicing_analyzer_free, sizeof(t_voicing_analyzer), CLASS_DEFAULT, A_GIMME, 0);
	class_addmethod(voicing_analyzer_class, (t_method)print_help, gensym("help"),0, 0);
	class_addlist(voicing_analyzer_class, (t_method)set_current_voices);
	

}
