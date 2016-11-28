/* hsd_svf~ external from the HSD-Library, University of Applied Science Duesseldorf
 Created by David Bau, 20.06.2015
 
 
 *******************
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 *******************
 
 
 The digital State-Variable-Filter is a standart second order multimode filter object with a dedicated output for Highpass, Bandpass and Lowpass.
 
 —>x
 
                yHP->                   yBP->                    yLP->
                 ^                       ^                        ^
->x              |                       |                        |
 o————————>(+)———o——>(*F1)——->(+)--------o--->(*F1)--->(+)--------o
            |-                 ^         |              ^         |
            |                  |         |              |         |
            |                  |         |              |         |
           (+)<----(*Q)--------o--[z-1]<-               |         |
            |   BP Feedback                             |         |
            |                                           |         |
             ———————————————————------------------------o--[z-1]--
                        LP Feedback
 
 The parameters frequency and resonance control directly the coefficients of the filter-algorithm. The output signals can be combined in Pd to realize other filter structures.
 
 
 
 
 */

#include "m_pd.h"
#include <math.h>

/* Default-Values */
#define DEFAULT_FREQUENCY 300
#define DEFAULT_RES 0.707
#define DEFAULT_TYPE "lowpass"

#define PI 3.14159265358979323846

/* The pointer to the class for "hsd_svf~" */
static t_class *hsd_svf_class;

/* Data struct */
typedef struct _hsd_svf
{
    /* The object itself */
    t_object obj;
    
    /* Sample Rate */
    t_float sr;
    
    /* dummy-float for CLASS_MAINSIGNALIN */
    t_float x_f;
    
    /* z-Elements. they store the output sample of the Lowpass and the Bandpass-Output */
    t_float z_yLP;
    t_float z_yBP;
    
    /* cutoff frequency */
    t_float fc;
    
    /* mapped frequency coefficient. it is calculated by 2*sin(PI * f_c / f_sr) */
    t_float F1;
    
    /* resonance coefficient, calculated in hsd_sfv_resonance whenever the third inlet receives a value. it is calculated by 1/resonance */
    t_float Q1;
    
}t_hsd_svf;

/* Function Prototypes */
void *hsd_svf_new (t_symbol *s, short argc, t_atom *argv);
void hsd_svf_dsp(t_hsd_svf *x, t_signal **sp, short *count);
t_int *hsd_svf_perform(t_int *w);
void hsd_svf_frequency(t_hsd_svf *x, t_floatarg f);
void hsd_svf_resonance(t_hsd_svf *x, t_floatarg f);
void hsd_svf_bang(t_hsd_svf *x);


/* Setup-Routine */
void hsd_svf_tilde_setup(void)
{
    hsd_svf_class = class_new(gensym("hsd_svf~"),
                            (t_newmethod)hsd_svf_new,
                            0,
                            sizeof(t_hsd_svf),
                            CLASS_DEFAULT,
                            A_GIMME,
                            0);
    
    CLASS_MAINSIGNALIN(hsd_svf_class,
                       t_hsd_svf,
                       x_f);
    
    class_addmethod(hsd_svf_class,
                    (t_method)hsd_svf_dsp,
                    gensym("dsp"),
                    0);
    
    /* add functions fo parameter changes */
    class_addmethod(hsd_svf_class,
                    (t_method)hsd_svf_frequency,
                    gensym("frequency"),
                    A_DEFFLOAT,
                    0);
    class_addmethod(hsd_svf_class,
                    (t_method)hsd_svf_resonance,
                    gensym("resonance"),
                    A_DEFFLOAT,
                    0);
    
    /* add the bang method to reset z-Elements */
    class_addbang(hsd_svf_class,hsd_svf_bang);
    
    post("hsd_svf~ by David Bau, HS Duesseldorf");
    
}

/* New-Instance-Routine */
void *hsd_svf_new (t_symbol *s, short argc, t_atom *argv)
{
    t_hsd_svf *x = (t_hsd_svf*)pd_new(hsd_svf_class);
    
    /* creating new active inlets for the two parameters frequency & resonance */
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("frequency"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("resonance"));
    
    /* creating three signal outlets, each outlet will output a different signal continously */
    outlet_new(&x->obj, gensym("signal"));  //Lowpass
    outlet_new(&x->obj, gensym("signal"));  //Highpass
    outlet_new(&x->obj, gensym("signal"));  //Bandpass
    
    /* init parameters & default values */
    x->sr = sys_getsr();
    x->z_yLP = 0;
    x->z_yBP = 0;

    t_float f = DEFAULT_FREQUENCY;
    t_float r = DEFAULT_RES;

    
    /* get the creation arguments & call the appropriate parameter-function*/
    if (argc>=2) {
        r = atom_getfloatarg(1, argc, argv);
        hsd_svf_resonance(x, r);
    }
    if (argc>=1) {
        f = atom_getfloatarg(0, argc, argv);
        hsd_svf_frequency(x, f);
    }
    
    
    
    
    return x;
}

/* this function calculates the coutoff-frequency-coefficient and is called whenever the second inlet receives a float value. there are two stages of sanity checking: first, the cutoff-frequency range is restricted to a reasonable range (20-20.000 Hz). after that, the coefficient F1 is calculated and is checked against the resonance coefficient Q1 for stabilty reasons. the state variable filter has the stability criteria "F1 < 2- Q1" [DAFX-Book, Zölzer]. So if F1 is greater tha 2-Q1, the parameter change will not take effect */
void hsd_svf_frequency(t_hsd_svf *x, t_floatarg f){
    
    t_float freq = f;
    
    /* sanity checking */
    if (freq < 20.0 ) {
        freq = 20;
    }
    if (freq > 20000.0) {
        freq = 20000.0;
    }
    
    //calculate the tuning parameter
    t_float f1 = 2.0*sin(PI * freq / x->sr);
    
    //check for filter-stability. F1 may not be greater than 2-Q1
    if (f1 < (2 - x->Q1)) {
        x->F1 = f1;
        x->fc = freq;
    }else{
        post("F1 > 2-Q!");
    }
    
}
/*this function calculates the resonance-coefficient and is called whenever the third inlet receives a float value. like the frequency-function, it has 2 stages of sanity checking and works quite similar */
void hsd_svf_resonance(t_hsd_svf *x, t_floatarg f){
    
    t_float res = f;
    
    /* sanity checking */
    if (res < 0.5 ) {
        res = 0.5;
    }
    if (res > 50.0) {
        res = 50.0;
    }
    
    //calculate resonance parameter
    t_float q1 = 1/res;
    
    //chek for filter stability. 2-Q1 may not be smaller than F1
    if (x->F1 < (2-q1)) {
        x->Q1 = q1;
    }else{
        post("2-Q < F1!");
    }
    
    
}

/* if the object receives a bang, the z-Elements are set to zero. This reset is a ultima ratio, in case the filter (despite all sanity checking) became unstable and blew up. */
void hsd_svf_bang(t_hsd_svf *x){
    
    x->z_yBP = 0.0;
    x->z_yLP = 0.0;
}

/* the dsp-init-routine */
void hsd_svf_dsp(t_hsd_svf *x, t_signal **sp, short *count)
{
    /* check if samplerate has changed */
    if(x->sr != sp[0]->s_sr){
        x->sr = sp[0]->s_sr;
        hsd_svf_frequency(x, x->fc);
    }
    
    /* add the objects signal processing to the signal-chain of puredata */
    dsp_add(hsd_svf_perform,    //the perform routine to execute
            6,                  //number of following parameters (the object + the channels, 1+5=6)
            x,                  //the object
            sp[0]->s_vec,       //inlet
            sp[1]->s_vec,       //outlet HP
            sp[2]->s_vec,       //outlet BP
            sp[3]->s_vec,       //outlet LP
            sp[0]->s_n);        //vector size
}

/* the perform routine */
t_int *hsd_svf_perform(t_int *w)
{
    /* get the signal vectors */
    t_hsd_svf *x =       (t_hsd_svf *) (w[1]);      //object data
    t_float *in =           (t_float *) (w[2]);     //input-vector
    t_float *outHP =        (t_float *) (w[3]);     //HP-output-vector (first outlet)
    t_float *outBP =        (t_float *) (w[4]);     //BP-output-vector (second outlet)
    t_float *outLP =        (t_float *) (w[5]);     //LP-output-vector (third outlet)
    t_int n =               w[6];                   //vector-size
    
    
    /* get needed data from data struct */
    t_float z_yLP = x->z_yLP;
    t_float z_yBP = x->z_yBP;
    t_float F = x->F1;
    t_float Q = x->Q1;
    
    /* init variables for intermediate stages */
    t_float yLP, yBP, yHP;
    
    /* DSP Loop */
    while (n--) {
        
        /* get the input */
        t_float input = *in++;
        
        /* calculate the first stage and therefore simultaneously the Highpass-output. The first stage is only the input plus the feedback path */
        yHP = input     - z_yLP     - Q * z_yBP;
        
        /* calculate the second stage / bandpass-output */
        yBP = F * yHP   + z_yBP;
         
        /* calculate the last stage / lowpass-output */
        yLP = F * yBP   + z_yLP;
        
        /* write the outputs to their outlets */
        *outHP++ = yHP;
        *outBP++ = yBP;
        *outLP++ = yLP;
        
        /* store the z-elements */
        z_yLP = yLP;
        z_yBP = yBP;
        
    }
    
    /* save the z-Elements to their data struct */
    x->z_yLP = z_yLP;
    x->z_yBP = z_yBP;
    
    return w+7;
}
