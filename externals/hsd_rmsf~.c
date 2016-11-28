/* hsd_rmsf~ external from the HSD-Library, University of Applied Science Duesseldorf
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
 
    This external generetes an envelope signal of an incoming audio signal. It uses one parameter, the averager-time in milliseconds. The computation of the rms-signal is done by the formula (DAFX, Zoelzer)
 
        y^2(n) = (1-TAV) * y^2(n-1) + TAV * x(n) * x(n);
    where y^2 is the squared output and TAV the time coefficient. From the squared output, the root is extracted before sending the signal to the outlet */



#include "m_pd.h"
#include <math.h>

/* default avereager time */
#define DEFAULT_RMS_MS 4

#define EULER 2.718281828459045


static t_class *hsd_rmsf_class;

/* Data Struct */
typedef struct _hsd_rmsf
{
    
    /* the object data itself */
    t_object obj;
    
    /* dummy float for CLASS_MAINSIGNALIN */
    t_float x_f;
    
    /* rms (averager) time for the envelope follower in ms */
    t_float t_rms;
    
    /* sampledelay-element. the output of the envelope follower is stored in this variable and is used for calculating the output value of the following sample */
    t_float xrms2_z1;
    
    /* samplerate */
    t_float sr;
    
}t_hsd_rmsf;

/* function prototypes */
void *hsd_rmsf_new (t_floatarg f);
void hsd_rmsf_dsp(t_hsd_rmsf *x, t_signal **sp, short *count);
t_int *hsd_rmsf_perform(t_int *w);


/* setup routine */
void hsd_rmsf_tilde_setup(void)
{
    hsd_rmsf_class = class_new(gensym("hsd_rmsf~"),
                            (t_newmethod)hsd_rmsf_new,
                            0,
                            sizeof(t_hsd_rmsf),
                            CLASS_DEFAULT,
                            A_DEFFLOAT,
                            0);
    
    CLASS_MAINSIGNALIN(hsd_rmsf_class,
                       t_hsd_rmsf,
                       x_f);
    
    class_addmethod(hsd_rmsf_class,
                    (t_method)hsd_rmsf_dsp,
                    gensym("dsp"),
                    0);
    
    post("hsd_rmsf~ by David Bau, University of Applied Sciences Duessldorf");
    
}


/* new-instance routine */
void *hsd_rmsf_new (t_floatarg f)
{
    t_hsd_rmsf *x = (t_hsd_rmsf*)pd_new(hsd_rmsf_class);
    
    /* creation of passive float inlet. the averager time is accessed by a passive inlet, so no sanity checking is used  */
    floatinlet_new(&x->obj, &x->t_rms);
    
    outlet_new(&x->obj, gensym("signal"));
    
    /* init samplerate, delay element and default value */
    x->sr = sys_getsr();
    x->xrms2_z1 = 0;
    
    x->t_rms = DEFAULT_RMS_MS;
    
    /* get creation argument */
    if (f) {
        x->t_rms = f;
    }
    
    return x;
}


/* the dsp-init-routine */
void hsd_rmsf_dsp(t_hsd_rmsf *x, t_signal **sp, short *count)
{
    /* check for sample-rate changes */
    if(x->sr != sp[0]->s_sr){
        x->sr = sp[0]->s_sr;
    }
    
    /* add the objects signal processing to the signal-chain of puredata */
    dsp_add(hsd_rmsf_perform,
            4,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[0]->s_n);
}

/* the perform routine */
t_int *hsd_rmsf_perform(t_int *w)
{
    /* get the signal vectors */
    t_hsd_rmsf *x = (t_hsd_rmsf *) (w[1]);              //object data
    t_float *in = (t_float *) (w[2]);                   //input-vector
    t_float *out = (t_float *) (w[3]);                  //output-vector
    t_int n = w[4];                                     //buffer-size
    
    /* get the delayed sample from the data struct */
    t_float xrms2_z1 = x->xrms2_z1;
    
    t_float x_in;
    
    /*calulate the time-constant */
    t_float TAV = 1 - pow(EULER, -2.2/(x->sr * x->t_rms * 0.001));
    
    /* DSP Loop */
    while (n--) {
        
        // get input sample
        x_in = *in++;
        
        // calculate squared rms-value
        xrms2_z1 = (1-TAV) * xrms2_z1 + TAV * x_in * x_in;
        
        // output squareroot of rms-value
        *out++ = sqrt(xrms2_z1);
    }
    
    /* store the values that are needed for the next buffer */
    x->xrms2_z1 = xrms2_z1;
    
    return w+5;
}
