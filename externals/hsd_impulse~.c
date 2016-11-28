/* hsd_impulse~ external from the HSD-Library, University of Applied Science Duesseldorf
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
 

 Simple stupid impulse generator.
 
 When a bang is sent to the object, it will send out n samples with a value 1, creating an impulse of the length n. n is determined by the parameter "length" and is set via creation arguments or a float inlet 
 
 */
 
 
#include "m_pd.h"

static t_class *hsd_impulse_class;

/* Data struct */
typedef struct _hsd_impulse
{
    /* the object data itself */
    t_object obj;
    
    /* dummy float for CLASS_MAINSIGNALIN */
    t_float x_f;
    
    /* sample rate */
    t_float sr;
    
    /* impulse length in samples */
    t_int length;
    
    /* running parameter. is set to "length", when a bang is received and is decremened by 1 every sample tick. as long as impulsecount is not zero, a sample-value of 1 will be send out to create a pulse */
    t_int impulsecount;
    
}t_hsd_impulse;

/* function prototypes */
void *hsd_impulse_new (t_floatarg f);
void hsd_impulse_dsp(t_hsd_impulse *x, t_signal **sp, short *count);
t_int *hsd_impulse_perform(t_int *w);
void hsd_impulse_length(t_hsd_impulse *x, t_floatarg f);
void hsd_impulse_bang(t_hsd_impulse *x);


/* setup routine */
void hsd_impulse_tilde_setup(void)
{
    hsd_impulse_class = class_new(gensym("hsd_impulse~"),
                            (t_newmethod)hsd_impulse_new,
                            0,
                            sizeof(t_hsd_impulse),
                            CLASS_DEFAULT,
                            A_DEFFLOAT,
                            0);
    /*
    No CLASS_MAINSIGNALIN, because no signal input is needed
    */
    
    class_addmethod(hsd_impulse_class,
                    (t_method)hsd_impulse_dsp,
                    gensym("dsp"),
                    0);
    
    /* add a method for setting the impulse length */
    class_addmethod(hsd_impulse_class,
                    (t_method)hsd_impulse_length,
                    gensym("length"),
                    A_DEFFLOAT,
                    0);
    
    /* add a bang for starting an impulse */
    class_addbang(hsd_impulse_class,hsd_impulse_bang);
    
    post("hsd_impulse~ by David Bau, HS Duesseldorf");
    
}

/* new-instance routine */
void *hsd_impulse_new(t_floatarg f)
{
    t_hsd_impulse *x = (t_hsd_impulse*)pd_new(hsd_impulse_class);
    
    /* the float inlet to the length */
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("length"));
    
    outlet_new(&x->obj, gensym("signal"));
    
    /* init parameters & get creation arguments */
    x->length = 0;
    
    if (f <= 0) { //sanity checking
        f = 1;
        post("negative or zero impulse not allowed, using 1");
    }
    x->length = (int)f;
    x->impulsecount = 0;


    
    
    return x;
}

/* function for setting the impulse length in samples */
void hsd_impulse_length(t_hsd_impulse *x, t_floatarg f){
    
    if (f <= 0) {
        f = 1;
        post("negative or zero impulse not allowed, using 1");
    }
    x->length = (int)f;
    
}

/* bang function for starting the impulse. when a bang is received, the impulsecount is set to the length-value. */
void hsd_impulse_bang(t_hsd_impulse *x){
    
    x->impulsecount = x->length;
    
}

/* the dsp-init-routine */
void hsd_impulse_dsp(t_hsd_impulse *x, t_signal **sp, short *count)
{
    /* add the objects signal processing to the signal-chain of puredata */
    dsp_add(hsd_impulse_perform,
            3,
            x,
            sp[0]->s_vec,
            sp[0]->s_n);
}

/* the perform routine */
t_int *hsd_impulse_perform(t_int *w)
{
    /* get the signal vectors */
    t_hsd_impulse *x =       (t_hsd_impulse *) (w[1]); //object data
    t_float *out =        (t_float *) (w[2]); //output-vector
    t_int n =               w[3];   //buffer size
    
    /* get needed data from data struct */
    t_int impulsecount = x->impulsecount;
    
    /* DSP Loop */
    while (n--) {
        
        if (impulsecount > 0) {
            impulsecount--;
            *out++ = 1;
        }  else{
            *out++ =  0;
        }
        
    }
    x->impulsecount = impulsecount;
    
    
    return w+4;
}
