/* hsd_peakf~ external from the HSD-Library, University of Applied Science Duesseldorf
 Created by David Bau, 16.05.2015

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
 
 
 This extnernal generetes an envelope signal of an incoming audio signal by measuring the peak-level. It uses two parameters, the attack- and relase time for wightening the peak-measurement. The computation of the peak-signal is done by the formula (DAFX, Zoelzer)

 y(n) = (1-AT) * y(n-1) + AT * x(n)   -> if the signal-level is rising (attack-phase)
 y(n) = (1-RT) * y(n-1)               -> if the signal-level is falling (release-phase)

 where AT/RT are the time coefficients. 
 
 NOTE: the time constants are calculated every sample block. It might be more efficient to calculate them in another function*/



#include "m_pd.h"
#include <math.h>

/* default attack- and release times */
#define DEFAULT_ATTACK_MS 1
#define DEFAULT_RELEASE_MS 20

#define EULER 2.718281828459045


static t_class *hsd_peakf_class;

/* Data Struct */
typedef struct _hsd_peakf
{
    /* the object data itself */
    t_object obj;

    /* dummy float for CLASS_MAINSIGNALIN */
    t_float x_f;
    
    /* attack and release times in ms */
    t_float t_a;
    t_float t_r;
    
    /* sampledelay-element. the output of the envelope follower is stored in this variable and is used for calculating the output value of the following sample */
    t_float xpeak_z1;
    
    /* samplerate */
    t_float sr;

}t_hsd_peakf;

/* function prototypes */
void *hsd_peakf_new (t_floatarg f1, t_floatarg f2);
void hsd_peakf_dsp(t_hsd_peakf *x, t_signal **sp, short *count);
t_int *hsd_peakf_perform(t_int *w);


/* setup routine */
void hsd_peakf_tilde_setup(void)
{
    hsd_peakf_class = class_new(gensym("hsd_peakf~"),
                            (t_newmethod)hsd_peakf_new,
                            0,
                            sizeof(t_hsd_peakf),
                            CLASS_DEFAULT,
                            A_DEFFLOAT,
                            A_DEFFLOAT,
                            0);
    
    CLASS_MAINSIGNALIN(hsd_peakf_class,
                       t_hsd_peakf,
                       x_f);
    
    class_addmethod(hsd_peakf_class,
                    (t_method)hsd_peakf_dsp,
                    gensym("dsp"),
                    0);
    
    post("hsd_peakf~ by David Bau, University of Applied Sciences Duessldorf");
    
}


/* new-instance routine */
void *hsd_peakf_new (t_floatarg f1, t_floatarg f2)
{
    t_hsd_peakf *x = (t_hsd_peakf*)pd_new(hsd_peakf_class);
    
    /* creation of passive float inlets. the attack and release times are accessed by a passive inlet, so no sanity checking is used  */
    floatinlet_new(&x->obj, &x->t_a);
    floatinlet_new(&x->obj, &x->t_r);
    
    outlet_new(&x->obj, gensym("signal"));
    
    /* init samplerate, delay element and default values */
    x->sr = sys_getsr();
    x->xpeak_z1 = 0;
    
    x->t_a = DEFAULT_ATTACK_MS;
    x->t_r = DEFAULT_RELEASE_MS;
    
    /* get creation arguments */
    if (f1) {
        x->t_a = f1;
    }
    if (f2) {
        x->t_r = f2;
    }
    
    return x;
}


/* the dsp-init-routine */
void hsd_peakf_dsp(t_hsd_peakf *x, t_signal **sp, short *count)
{
    /* check for sample-rate changes */
    if(x->sr != sp[0]->s_sr){
        x->sr = sp[0]->s_sr;
    }
    
    /* add the objects signal processing to the signal-chain of puredata */
    dsp_add(hsd_peakf_perform,
            4,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[0]->s_n);
}

/* the perform routine */
t_int *hsd_peakf_perform(t_int *w)
{
    /* get the signal vectors */
    t_hsd_peakf *x = (t_hsd_peakf *) (w[1]);            //object data
    t_float *in = (t_float *) (w[2]);                   //input-vector
    t_float *out = (t_float *) (w[3]);                  //output-vector
    t_int n = w[4];                                     //buffer-size
    
    /* get the delayed sample from the data struct */
    t_float xpeak_z1 = x->xpeak_z1;
    
    t_float a;
    
    /* calulate the time-constants */
    t_float AT = 1 - pow(EULER, -2.2/(x->sr * x->t_a * 0.001));
    t_float RT = 1 - pow(EULER, -2.2/(x->sr * x->t_r * 0.001));
    
    /* DSP Loop */
    while (n--) {
        
        // get input sample (absolute value)
        a = fabs(*in++);
        
        // calculate the peak-value
        if (a > xpeak_z1) {
            xpeak_z1 = (1-AT) * xpeak_z1 + AT * a;
        }else{
            xpeak_z1 = (1-RT) * xpeak_z1;
        }
        
        // output peak value
        *out++ = xpeak_z1;
    }
    
    /* store the values that are needed for the next buffer */
    x->xpeak_z1 = xpeak_z1;
    
    return w+5;
}
