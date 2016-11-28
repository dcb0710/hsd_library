/* hsd_biquad_coefficients external from the HSD-Library, University of Applied Science Duesseldorf
 Created by David Bau, 12.06.2015
 
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
 
 
 This is a modified version of the hsd_biquad-external. It just calculates the coefficients and sends them out through the 5 outlets. there is no signal processing done by this external */

#include "m_pd.h"
#include <math.h>

/* Default-Values */
#define DEFAULT_FREQUENCY 300
#define DEFAULT_RES 0.707
#define DEFAULT_TYPE "lowpass"

/* The pointer to the class for "hsd_biquad~" */
static t_class *hsd_biquad_coefficients_class;

/* Data struct */
typedef struct _hsd_biquad_coefficients
{
    /* The object itself */
    t_object obj;
    
    /* Sample Rate */
    t_float sr;
    
    /* Biquad-Parameters. They are set via their dedicated functions "hsd_biquad_frequency()", "hsd_biquad_resonance()" and "hsd_biquad_symbol()" */
    t_symbol *type;
    t_float typenumber;
    t_float frequency;
    t_float resonance;
    
    /*outlets. they have to obe stored in the data struct with the "t_outlet" variable. then other functions can use them by writing on them like on variables. the creation of the outlet is in the same place as the inlets, in the new-function */
    t_outlet *b0_out, *b1_out, *b2_out,*a1_out, *a2_out;
    
}t_hsd_biquad_coefficients;

/* Function Prototypes */
void *hsd_biquad_coefficients_new (t_symbol *s, short argc, t_atom *argv);
void hsd_biquad_coefficients_float(t_hsd_biquad_coefficients *x, t_floatarg f);
void hsd_biquad_coefficients_resonance(t_hsd_biquad_coefficients *x, t_floatarg f);
void hsd_biquad_coefficients_symbol(t_hsd_biquad_coefficients *x, t_symbol *s);
void hsd_biquad_coefficients_calculate_coeffs(t_hsd_biquad_coefficients *x);


/* Setup-Routine */
void hsd_biquad_coefficients_setup(void)
{

    hsd_biquad_coefficients_class = class_new(gensym("hsd_biquad_coefficients"),
                            (t_newmethod)hsd_biquad_coefficients_new,
                            0,
                            sizeof(t_hsd_biquad_coefficients),
                            CLASS_DEFAULT,
                            A_GIMME,
                            0);

    /* No CLASS_MAINSIGNALIN, No addmethod(dsp) */
    
    /* add the symbol method for setting the type */
    class_addsymbol(hsd_biquad_coefficients_class,
                    hsd_biquad_coefficients_symbol);
    
    /* add a standart float method for setting the frequency. this differs from the hsd_biquad~-object, because the first inlet here is used for frequency, not for signal. the first inlet is always created, we can´t create a new inlet with a selector. */
    class_addfloat(hsd_biquad_coefficients_class,
                   hsd_biquad_coefficients_float);
    
    /* add the method for resonance. */
    class_addmethod(hsd_biquad_coefficients_class,
                    (t_method)hsd_biquad_coefficients_resonance,
                    gensym("resonance"),
                    A_DEFFLOAT,
                    0);
    
    post("hsd_biquad_coefficients~ by David Bau, HS Duesseldorf");
    
}

void *hsd_biquad_coefficients_new (t_symbol *s, short argc, t_atom *argv)
{
    t_hsd_biquad_coefficients *x = (t_hsd_biquad_coefficients*)pd_new(hsd_biquad_coefficients_class);
    
    //inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("frequency"));
    
    /* only add 2 new inlets, because the first inlet (for frequency) is already created */
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("resonance"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("symbol"), gensym("symbol"));
    
    /* "outlet_new" has a return value (the t_outlet) that we need to store in the data struct. Thus, sending out a float through them is just like writing a variable ( see end of hsd_biquad_calculate_coeffs() ). the first argument of the function call is the object instance the outlets are added to, the second argument is the type of variable, that the output will be used for (in our case they are all float oulets). the order of creation also matters, as they appear in PD exactly this way */
    x->b0_out=outlet_new(&x->obj, &s_float);
    x->b1_out=outlet_new(&x->obj, &s_float);
    x->b2_out=outlet_new(&x->obj, &s_float);
    x->a1_out=outlet_new(&x->obj, &s_float);
    x->a2_out=outlet_new(&x->obj, &s_float);
    
    /* get samplerate */
    x->sr = sys_getsr();
    
    /* init defaults */
    x->type = gensym(DEFAULT_TYPE);
    x->frequency = DEFAULT_FREQUENCY;
    x->resonance = DEFAULT_RES;
    
    /* get the creation arguments: frequency, resonance, type */
    if (argc>=3) {
        x->type = atom_getsymbolarg(2, argc, argv);
    }
    if (argc>=2) {
        x->resonance = atom_getfloatarg(1, argc, argv);
    }
    if (argc>=1) {
        x->frequency = atom_getfloatarg(0, argc, argv);
    }
    
    /* set the filter-type and thereby start the initial calculation of coefficients */
    hsd_biquad_coefficients_symbol(x, x->type);
    
    return x;
}

/* this function is called whenever the object receives a float . Since no other inlet will get floats (note: if a float is sent to the second (resonance-)inlet, it is substituted by the selector "resonance" and hsd_biquad_coefficients_float() is NOT called!), this function will only be called by the first inlet. The purpose of this function is to do sanity-checking and write the incoming float value to the frequency-parameter in the data struct, then execute the recalculation of coefficients */
void hsd_biquad_coefficients_float(t_hsd_biquad_coefficients *x, t_floatarg f){
    
    t_float freq = f;
    if (freq < 20.0 ) {
        freq = 20;
    }
    if (freq > 20000.0) {
        freq = 20000.0;
    }
    x->frequency = freq;
    
    /* start recalculation */
    hsd_biquad_coefficients_calculate_coeffs(x);
    
}

/* this function is bound to the second inlet with a selector (see the inlet_new-call in the hsd_biquad_new-function). whenever the second inlet receives a float, this function will be called and will be passed the float.The purpose of this function is to do sanity-checking and write the incoming float value to the resonance-parameter in the data struct, then execute the recalculation of coefficients  */
void hsd_biquad_coefficients_resonance(t_hsd_biquad_coefficients *x, t_floatarg f){
    
    t_float res = f;
    if (res < 0.1 ) {
        res = 0.1;
    }
    if (res > 20.0) {
        res = 20.0;
    }
    x->resonance = res;
    
    /* start recalculation */
    hsd_biquad_coefficients_calculate_coeffs(x);
    
}

/* this function is triggered whenever the object receives a symbol message. because it is the only function that expects symbols, a unique selector was not necessary(unlike the resonance- and frequency-function). The purpose of this function is to recognize the selected type by a string comparison and select the appropriate typenumber, then execute the recalculation of coefficients  */
void hsd_biquad_coefficients_symbol(t_hsd_biquad_coefficients *x, t_symbol *s){
    
    if (s == gensym("lowpass")) {
        x->type = gensym("lowpass");
        x->typenumber = 0;
    }
    else if (s == gensym("highpass")) {
        x->type = gensym("highpass");
        x->typenumber = 1;
    }
    else if (s == gensym("bandpass")) {
        x->type = gensym("bandpass");
        x->typenumber = 2;
    }
    else if (s == gensym("bandreject")) {
        x->type = gensym("bandreject");
        x->typenumber = 3;
    }
    else if (s == gensym("allpass")) {
        x->type = gensym("allpass");
        x->typenumber = 4;
    }
    else{
        post("%s is not a legal type, lowpass is used", s->s_name);
        x->type = gensym("lowass");
        x->typenumber = 0;
    }
    
    hsd_biquad_coefficients_calculate_coeffs(x);
}


/* this function is called whenever a parameter (frequency, Q or type) has changed. the coefficients are recalculated*/
void hsd_biquad_coefficients_calculate_coeffs(t_hsd_biquad_coefficients *x){
    
    /* define coefficients. they are send out through the outlets at the end of the function and therefore don´t have to be stored anywhere */
    t_float b0, b1, b2, a1, a2;

    /* get samplerate from data struct */
    t_float sr = x->sr;
    
    /* get resonance and frequency from data struct and calculate K (-> Udo Zoelzer: DAFX-Book) */
    t_float K = tanf(M_PI * x->frequency / sr) ;
    t_float Q = x->resonance;
    
    /* precalculate division factor that will be used by almost every coeffcient */
    t_float omega = 1.0 / (K*K*Q + K + Q);
    
    /* select matching type and calculate the coefficients (-> Udo Zoelzer: DAFX-Book) */
    int typenumber = (int)x->typenumber;
    switch (typenumber) {
        case 0: //Lowpass
            b0 = K * K * Q * omega;      //b0
            b1 = 2.0 * b0;               //b1
            b2 = b0;                     //b2
            a1 = b1 - 2.0 * Q * omega;   //a1
            a2 = b0 + (Q-K)*omega;       //a2
            break;
        case 1: //Highpass
            b0 = Q * omega;
            b1 = -2.0 * b0;
            b2 = b0;
            a1 = 2.0*(K*K - 1.0) * b0;
            a2 = (K*K*Q - K + Q) * omega;
            break;
        case 2: //Bandpass
            b0 = K * omega;
            b1 = 0;
            b2 = -b1;
            a1 = 2.0*Q*(K*K-1.0)*omega;
            a2 = (K*K*Q - K + Q) * omega;
            break;
        case 3: //Bandreject
            b0 = Q*(1.0+K*K) * omega;
            b1 = 2.0*Q*(K*K - 1.0) * omega;
            b2 = b0;
            a1 = b1;
            a2 = (K*K*Q - K + Q) * omega;
            break;
        case 4: //Allpass
            b0 = (K*K*Q - K + Q) * omega;
            b1 = 2.0*Q*(K*K - 1.0) * omega;
            b2 = 1.0;
            a1 = b1;
            a2 = b0;
            break;
        default: //In case of error
            b0 = 0;
            b1 = 0;
            b2 = 0;
            a1 = 0;
            a2 = 0;
            break;
    }
    
    /* send the calculated coefficients to the outlets with "outlet_float()". the first argument specifies the outlet and the second argument the float that is supposed to be sent*/
    
    outlet_float(x->b0_out, b0);
    outlet_float(x->b1_out, b1);
    outlet_float(x->b2_out, b2);
    outlet_float(x->a1_out, a1);
    outlet_float(x->a2_out, a2);
    
}


