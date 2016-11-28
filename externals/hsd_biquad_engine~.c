/* hsd_biquad_engine~ external from the HSD-Library, University of Applied Science Duesseldorf
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
 
 
 
 This is a modified version of the hsd_biquad-external. It does not calculate the coefficients itself, instead they are set directly by 5 inlets or with creation arguments. The inlets are passive inlets and write directly to the variables, so there are no restrictions to the coefficients.  */

#include "m_pd.h"

/* The pointer to the class for "hsd_biquad~" */
static t_class *hsd_biquad_engine_class;


/* Data struct */
typedef struct _hsd_biquad_engine
{
    /* The object itself */
    t_object obj;
    
    /* z-Elements. They are used in the DSP-Loop. They can store one sample-(float-)value which can be read again in the next sample-cycle. this corresponds to a [z^-1] - delay. you can add multiple [z^-1]-delay together by writing the value of a z-Element into the next z-Element (for example "z2 = z1;") at the end of each cycle*/
    t_float z1, z2;
    
    /* the coefficients: instead of using an array, the coefficients in this case are stored seperatly, because the are set individually by floatinlets */
    t_float b0, b1, b2, a1, a2;

    /* dummy-float for CLASS_MAINSIGNALIN */
    t_float x_f;
    
}t_hsd_biquad_engine;


/* Function Prototypes */
void *hsd_biquad_engine_new (t_symbol *s, short argc, t_atom *argv);
void hsd_biquad_engine_dsp(t_hsd_biquad_engine *x, t_signal **sp, short *count);
t_int *hsd_biquad_engine_perform(t_int *w);

/* Setup-Routine */
void hsd_biquad_engine_tilde_setup(void)
{
    hsd_biquad_engine_class = class_new(gensym("hsd_biquad_engine~"),
                            (t_newmethod)hsd_biquad_engine_new,
                            0,
                            sizeof(t_hsd_biquad_engine),
                            CLASS_DEFAULT,
                            A_GIMME,
                            0);
    
    CLASS_MAINSIGNALIN(hsd_biquad_engine_class,
                       t_hsd_biquad_engine,
                       x_f);
    
    class_addmethod(hsd_biquad_engine_class,
                    (t_method)hsd_biquad_engine_dsp,
                    gensym("dsp"),
                    0);

    post("hsd_biquad_engine~ by David Bau, HS Duesseldorf");
    
}

/* New-Instance-Routine */
void *hsd_biquad_engine_new (t_symbol *s, short argc, t_atom *argv)
{
    t_hsd_biquad_engine *x = (t_hsd_biquad_engine*)pd_new(hsd_biquad_engine_class);
    
    /* creation of floatinlets for every coefficient, so they can be directly accessed from outside. this way the filter is "unprotected", it will calculate the filter even with insane parameters. it can blow up. seriously. */
    floatinlet_new(&x->obj, &x->b0);
    floatinlet_new(&x->obj, &x->b1);
    floatinlet_new(&x->obj, &x->b2);
    floatinlet_new(&x->obj, &x->a1);
    floatinlet_new(&x->obj, &x->a2);
    
    outlet_new(&x->obj, gensym("signal"));

    /* init the variables */
    x->z1 = 0;
    x->z2 = 0;

    x->b0 = 0;
    x->b1 = 0;
    x->b2 = 0;
    x->a1 = 0;
    x->a2 = 0;
    
    /* get the creation arguments: b0, b1, b2, a1, a2 */
    if (argc>=5) {
        x->a2 = atom_getfloatarg(4, argc, argv);
    }
    if (argc>=4) {
        x->a1 = atom_getfloatarg(3, argc, argv);
    }
    if (argc>=3) {
        x->b2 = atom_getfloatarg(2, argc, argv);
    }
    if (argc>=2) {
        x->b1 = atom_getfloatarg(1, argc, argv);
    }
    if (argc>=1) {
        x->b0 = atom_getfloatarg(0, argc, argv);
    }
    
    return x;
}

void hsd_biquad_engine_dsp(t_hsd_biquad_engine *x, t_signal **sp, short *count)
{
    //get the signal vectors
    dsp_add(hsd_biquad_engine_perform,
            4,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[0]->s_n);
}

t_int *hsd_biquad_engine_perform(t_int *w)
{
    t_hsd_biquad_engine *x =       (t_hsd_biquad_engine *) (w[1]);  //the data struct
    t_float *in =           (t_float *) (w[2]);                     //input-buffer
    t_float *out =          (t_float *) (w[3]);                     //output-buffer
    t_int n =               w[4];                                   //buffer-length
    
    //get the z-Elements from the data struct
    t_float z1 = x->z1;
    t_float z2 = x->z2;
    
    t_float u;
    
    while (n--) {
        
        // calculate the filter!
        u = *in++ - x->a1*z1 - x->a2*z2;        //Feedback-Path with a1 & a2
        *out++ = x->b0*u + x->b1*z1 + x->b2*z2; //Feedforward-Path with b0, b1 & b2
        
        //shift the z-Elements
        z2 = z1;
        z1 = u;
        
    }
    
    //store the z-Elemtents back into the data struct
    x->z1 = z1;
    x->z2 = z2;
    
    return w+5;
}
