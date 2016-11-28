/* hsd_biquad~ external from the HSD-Library, University of Applied Science Duesseldorf
 Created by David Bau, 10.06.2015
 
 
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
 
 
 This is a standart biquad-filter (Direct Form 2). the coefficients are set by inlets (frequency, Q, type). there are 5 possible types: lowpass, highpass, bandpass, bandreject and allpass. the calculation of the coefficientes is done using formulas from Udo Zoelzer´s DAFX-Book in the calulate_coeffs-function. they are stored in the coeffs[]-array. the perform-routine reads this array and does the filter processing */

#include "m_pd.h"
#include <math.h>

/* Default-Values */
#define DEFAULT_FREQUENCY 300
#define DEFAULT_RES 0.707
#define DEFAULT_TYPE "lowpass"

/* The pointer to the class for "hsd_biquad~" */
static t_class *hsd_biquad_class;


/* Data struct */
typedef struct _hsd_biquad
{
    /* The object itself */
    t_object obj;
    
    /* Sample Rate */
    t_float sr;
    
    /* z-Elements. They are used in the DSP-Loop. They can store one sample-(float-)value which can be read again in the next sample-cycle. this corresponds to a [z^-1] - delay. you can add multiple [z^-1]-delay together by writing the value of a z-Element into the next z-Element (for example "z2 = z1;") at the end of each cycle*/
    t_float z1;
    t_float z2;
    
    /* Biquad-Parameters. They are set via their dedicated functions "hsd_biquad_frequency()", "hsd_biquad_resonance()" and "hsd_biquad_symbol()" */
    t_symbol *type;
    t_float typenumber;
    t_float frequency;
    t_float resonance;
    
    /* array of coefficients. one could have used five single float variables instead. but they are always accessed in group, so an array seemed appropriate */
    t_float coeffs[5]; //0 = b0, 1 = b1, 2 = b2, 3 = a1, 4 = a2
    
    /* help-array of coefficients */
    t_float oldcoeffs[5];
    
    /* this flag is set by the hsd_biquad_calculate_coeffs()-function when it is calculating the new coefficients and is read by the dsp-loop */
    t_int calculating_flag;
    
    /* dummy-float for CLASS_MAINSIGNALIN */
    t_float x_f;
}t_hsd_biquad;


/* Function Prototypes */
void *hsd_biquad_new (t_symbol *s, short argc, t_atom *argv);
void hsd_biquad_dsp(t_hsd_biquad *x, t_signal **sp, short *count);
t_int *hsd_biquad_perform(t_int *w);
void hsd_biquad_frequency(t_hsd_biquad *x, t_floatarg f);
void hsd_biquad_resonance(t_hsd_biquad *x, t_floatarg f);
void hsd_biquad_symbol(t_hsd_biquad *x, t_symbol *s);
void hsd_biquad_calculate_coeffs(t_hsd_biquad *x);


/* Setup-Routine */
void hsd_biquad_tilde_setup(void)
{
    hsd_biquad_class = class_new(gensym("hsd_biquad~"),
                            (t_newmethod)hsd_biquad_new,
                            0,
                            sizeof(t_hsd_biquad),
                            CLASS_DEFAULT,
                            A_GIMME,
                            0);
    
    CLASS_MAINSIGNALIN(hsd_biquad_class,
                       t_hsd_biquad,
                       x_f);
    
    class_addmethod(hsd_biquad_class,
                    (t_method)hsd_biquad_dsp,
                    gensym("dsp"),
                    0);
    
    /* add functions for setting the parameters. frequency & resonance are both expecting floats, so they have dedicated functions wich are accessed by a selector. the type is the only parameter that expects a symbol as input, so it doesn´t need a dedicated function and is set within the symbol-function (which is added via "class_addsymbol", which reacts to every symbol received by the object */
    
    class_addmethod(hsd_biquad_class,
                    (t_method)hsd_biquad_frequency,
                    gensym("frequency"),
                    A_DEFFLOAT,
                    0);
    class_addmethod(hsd_biquad_class,
                    (t_method)hsd_biquad_resonance,
                    gensym("resonance"),
                    A_DEFFLOAT,
                    0);
    class_addsymbol(hsd_biquad_class,
                    hsd_biquad_symbol);

    
    post("hsd_biquad~ by David Bau, HS Duesseldorf");
    
}

/* New-Instance-Routine */
void *hsd_biquad_new (t_symbol *s, short argc, t_atom *argv)
{
    t_hsd_biquad *x = (t_hsd_biquad*)pd_new(hsd_biquad_class);
    
    /* add inlets for every paramter. frequency & resonance have dedicated functions, they are accessed by a selector. if the second inlet (->the first created inlet, frequency) receives a float message, it is handled like a message "frequency" wich will trigger the hsd_biquad_frequency-function  */
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("frequency"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("resonance"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("symbol"), gensym("symbol"));
    
    /* add the signal outlet */
    outlet_new(&x->obj, gensym("signal"));
    
    /* init the parameters & default values */
    x->sr = sys_getsr();
    x->z1 = 0;
    x->z2 = 0;
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
    hsd_biquad_symbol(x, x->type);
    
    return x;
}

/* this function is bound to the second inlet with a selector (see the inlet_new-call in the hsd_biquad_new-function). whenever the second inlet receives a float, this function will be called and will be passed the float. The purpose of this function is to do sanity-checking and write the incoming float value to the frequency-parameter in the data struct, then execute the recalculation of coefficients */
void hsd_biquad_frequency(t_hsd_biquad *x, t_floatarg f){
    
    t_float freq = f;
    if (freq < 20.0 ) {
        freq = 20;
    }
    if (freq > 20000.0) {
        freq = 20000.0;
    }
    x->frequency = freq;
    
    /* start recalculation */
    hsd_biquad_calculate_coeffs(x);
    
}

/* this function is bound to the third inlet with a selector (see the inlet_new-call in the hsd_biquad_new-function). whenever the third inlet receives a float, this function will be called and will be passed the float.The purpose of this function is to do sanity-checking and write the incoming float value to the resonance-parameter in the data struct, then execute the recalculation of coefficients  */
void hsd_biquad_resonance(t_hsd_biquad *x, t_floatarg f){
    
    t_float res = f;
    if (res < 0.1 ) {
        res = 0.1;
    }
    if (res > 20.0) {
        res = 20.0;
    }
    x->resonance = res;
    
    /* start recalculation */
    hsd_biquad_calculate_coeffs(x);
    
}

/* this function is triggered whenever the object receives a symbol message. because it is the only function that expects symbols, a unique selector was not necessary(unlike the resonance- and frequency-function. The purpose of this function is to recognize the selected type by a string comparison and select the appropriate typenumber, then execute the recalculation of coefficients  */
void hsd_biquad_symbol(t_hsd_biquad *x, t_symbol *s){
    
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
    
    /* start recalculation */
    hsd_biquad_calculate_coeffs(x);
}

/* this function is called whenever a parameter (frequency, Q or type) has changed. the coefficients are recalculated*/
void hsd_biquad_calculate_coeffs(t_hsd_biquad *x){
    
    /* getting the arrays from the data struct */
    t_float *coeffs = x->coeffs;
    t_float *oldcoeffs = x->oldcoeffs;
    
    /* copy the coefficients to the help-array "oldcoeffs".  */
    memcpy(oldcoeffs, coeffs, 5.0 * sizeof(t_float));
    
    /* set the calculation flag. this flag will be read by the perform-routine. if it is set, the perform routine will read form the oldcoeffs-array */
    x->calculating_flag = 1;
    
    /* get samplerate */
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
            coeffs[0] = K * K * Q * omega;             //b0
            coeffs[1] = 2.0 * coeffs[0];               //b1
            coeffs[2] = coeffs[0];                     //b2
            coeffs[3] = coeffs[1] - 2.0 * Q * omega;   //a1
            coeffs[4] = coeffs[0] + (Q-K)*omega;       //a2
            break;
        case 1: //Highpass
            coeffs[0] = Q * omega;
            coeffs[1] = -2.0 * coeffs[0];
            coeffs[2] = coeffs[0];
            coeffs[3] = 2.0*(K*K - 1.0) * coeffs[0];
            coeffs[4] = (K*K*Q - K + Q) * omega;
            break;
        case 2: //Bandpass
            coeffs[0] = K * omega;
            coeffs[1] = 0;
            coeffs[2] = -coeffs[1];
            coeffs[3] = 2.0*Q*(K*K-1.0)*omega;
            coeffs[4] = (K*K*Q - K + Q) * omega;
            break;
        case 3: //Bandreject
            coeffs[0] = Q*(1.0+K*K) * omega;
            coeffs[1] = 2.0*Q*(K*K - 1.0) * omega;
            coeffs[2] = coeffs[0];
            coeffs[3] = coeffs[1];
            coeffs[4] = (K*K*Q - K + Q) * omega;
            break;
        case 4: //Allpass
            coeffs[0] = (K*K*Q - K + Q) * omega;
            coeffs[1] = 2.0*Q*(K*K - 1.0) * omega;
            coeffs[2] = 1.0;
            coeffs[3] = coeffs[1];
            coeffs[4] = coeffs[0];
            break;
        default: //In case of error
            coeffs[0] = 0;
            coeffs[1] = 0;
            coeffs[2] = 0;
            coeffs[3] = 0;
            coeffs[4] = 0;
            break;
    }
    
    /* reset the calculation_flag, so the recalculated array "coeffs" will be read again by the perform routine */
    x->calculating_flag = 0;
}




void hsd_biquad_dsp(t_hsd_biquad *x, t_signal **sp, short *count)
{
    /* check if the sampe-rate has changed*/
    if(x->sr != sp[0]->s_sr){
        x->sr = sp[0]->s_sr;
        /* the coefficients have to be recalculated */
        hsd_biquad_calculate_coeffs(x);
        
    }
    dsp_add(hsd_biquad_perform,
            4,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[0]->s_n);
}


t_int *hsd_biquad_perform(t_int *w)
{
    //get the signal vectors
    t_hsd_biquad *x =       (t_hsd_biquad *) (w[1]);    //the data struct
    t_float *in =           (t_float *) (w[2]);         //input-buffer
    t_float *out =          (t_float *) (w[3]);         //output-buffer
    t_int n =               w[4];                       //buffer-length
    
    //get the z-Elements from the data struct
    t_float z1 = x->z1;
    t_float z2 = x->z2;
    
    //get the coefficients and the help-coefficients
    t_float *coeffs = x->coeffs;
    t_float *oldcoeffs = x->oldcoeffs;
    
    t_float u;
    
    while (n--) {
        
        /* check if in hsd_biquad_calculate_coeffs a recalculation is in progress*/
        if (x->calculating_flag) {
            /* recalculation in progress => use the help-array */
            u = *in++ - oldcoeffs[3]*z1 - oldcoeffs[4]*z2;                  //Feedback-Path with a1 & a2
            *out++ = oldcoeffs[0]*u + oldcoeffs[1]*z1 + oldcoeffs[2]*z2;    //Feedforward-Path with b0, b1 & b2

        }else{
            /* no recalculation in progress => use the orignal array */
            u = *in++ - coeffs[3]*z1 - coeffs[4]*z2;                        //Feedback-Path with a1 & a2
            *out++ = coeffs[0]*u + coeffs[1]*z1 + coeffs[2]*z2;             //Feedforward-Path with b0, b1 & b2
        }
        
        //shift the z-Elements
        z2 = z1;
        z1 = u;
        
        
    }
    
    //store the z-Elemtents back into the data struct
    x->z1 = z1;
    x->z2 = z2;
    
    return w+5;
}
