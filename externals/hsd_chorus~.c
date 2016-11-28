/* hsd_chorus~ external from the HSD-Library, University of Applied Science Duesseldorf
 Created by David Bau, 13.03.2015

 
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
 
 
 This is a basic stereo chorus effect, consisting of two modulated delay lines (hsd_vibrato~). They are (similarily to hsd_vibrato~) modulated with a Sinewave-LFO, but the right channel gets a 90° phase shift on it´s lfo, so the modulation effect is out of phase. This simple trick creates a "chasing" stereo effect (the right channel "chases" the left). It is derived from the Book "Designing Audio Effect Plugins in C++" by Will Pirkle (Chapter 10).
 

    STEREO QUADRATURE CHORUS (PIRKLE)
                 ______________________________
                |                              |
  —>x_left      |      ____________________    v     y_left—>
        o—————--°-———>|______z-D1__________|——(+)————>o
                                  ^
                                  |sin(2pi*f)
                                  |
                                (LFO)
                                  |
                                  |sin(2pi*f+90°)
  —>x_right            ___________v________         y_right—>
        o—————--.-———>|______z-D2__________|——(+)——-->o
                |                              ^
                |______________________________|
 
 It has four control paramters:
    - D1 and D2 are controlling the individual delay-lengths
        -> "depth_ms_l" and "depth_ms_r"
    - "frequency" controls the frequency of the LFO
    - "dry_wet" controls the balance between the dry signal and the output of the delay line
        ->  0   - only dry
            50  - 50/50 mix
            100 - only wet
 
 Note that many parameters had to be doubled to be used for both channels. This is the reason for the length of the code.
 
 */

#include "m_pd.h"
#include "math.h"

/* defaults */
#define DELMAX 40

#define PI 3.1415926536

/* data struct */
typedef struct _hsd_chorus{
    
    /* the object data itself */
    t_object obj;
    
    /* sample rate */
    t_float sr;
    
    /* the length of the complete delay-line in samples. the maximum delay time is defined by DELMAX 100 milliseconds, therefore the delayline is always allocated with enough samples to store 100ms of audio. this value is always in dependance of the samplerate and has to be recalculated when the sample rate changes */
    t_float delay_line_length;
    
    /* the length of the delay-line in bytes. it is proportional to delay_line_length, which is just mulitplied by the byte-size of a float variable. it is important to store the bytes, because when the object is destroyed at the end of runtime, the allocated memory has to be freed again */
    t_int delay_bytes;
    
    /* the pointer to the delay-line itself. when the memory of the delay-line is allocated, a pointer to this memory (to the first entry) is returned and stored in this variable. */
    t_float *delay_line_l;
    t_float *delay_line_r;
    
    /* the current position of the write-pointer. it is incremented with every sample-tick in the dsp-loop. it indicates the position, where the input is written to the delay-line. */
    t_int write_index_l;
    t_int write_index_r;
    
    /* the current position of the read-pointer. it "follows" the write pointer skimming thorugh the delay-line with a displacement of delay_length. this displacement is achieved by subtracting the delay_length from the write-pointer */
    t_int read_index_l;
    t_int read_index_r;
    
    /* dummy float for CLASS_MAINSIGNALIN */
    t_float x_f;
    
    /* modulation of the delay_time in samples and milliseconds */
    t_float depth_l;
    t_float depth_ms_l;
    
    /* modulation of the delay_time of the right channel in samples and milliseconds */
    t_float depth_r;
    t_float depth_ms_r;
    
    /* frequency of the LFO */
    t_float frequency;
    
    /* time interval in samples of the LFO */
    t_float cycle_length;
    
    /* oscillator phase. it runs from 0 up to the cycle-length and is incremented every sample tick by 1.0 */
    t_float phase;
    
    /* mix dry and wet signal. range 0 to 1. dry = 1-wet   */
    t_float dry, wet;
    
    
}t_hsd_chorus;

static t_class *hsd_chorus_class;


/* basic function prototypes */
void *hsd_chorus_new(t_symbol *s, short argc, t_atom *argv);
void hsd_chorus_dsp(t_hsd_chorus *x, t_signal **sp);
t_int *hsd_chorus_perform(t_int *w);
void hsd_chorus_free(t_hsd_chorus *x);
/* prototypes parameter-functions */
void hsd_chorus_depth_l(t_hsd_chorus *x, t_floatarg f);
void hsd_chorus_depth_r(t_hsd_chorus *x, t_floatarg f);
void hsd_chorus_frequency(t_hsd_chorus *x, t_floatarg f);
void hsd_chorus_drywet(t_hsd_chorus *x, t_floatarg f);


/* function for setting the modulation depth, called by the third inlet, performs sanity checking */
void hsd_chorus_depth_l(t_hsd_chorus *x, t_floatarg f){
    
    t_float depth_ms = f;
    
    // sanity checking
    if(depth_ms > DELMAX){
        depth_ms=DELMAX;
        
    }
    if (depth_ms < 0) {
        depth_ms = 0;
    }
    x->depth_ms_l = depth_ms;
    x->depth_l = x->sr * x->depth_ms_l/1000;
    
    
}

void hsd_chorus_depth_r(t_hsd_chorus *x, t_floatarg f){
    
    t_float depth_ms = f;
    
    // sanity checking
    if(depth_ms > DELMAX){
        depth_ms=DELMAX;
        
    }
    if (depth_ms < 0) {
        depth_ms = 0;
    }
    x->depth_ms_r = depth_ms;
    x->depth_r = x->sr * x->depth_ms_r/1000;
    
    
}

void hsd_chorus_frequency(t_hsd_chorus *x, t_floatarg f){
    
    t_float frequency = f;
    
    // sanity checking
    if(frequency <= 0){
        error("hsd_chorus~: frequency must be nonzero & positive");
    }else{
        x->cycle_length = x->sr/frequency;
        x->frequency = frequency;
        post("frequency: %f, cycle_length: %f", x->frequency, x->cycle_length);
    }
    
}

void hsd_chorus_drywet(t_hsd_chorus *x, t_floatarg f){
    
    //create intermediate value
    t_float dry_wet = f;
    
    // sanity checking
    if(dry_wet < 0){
        dry_wet = 0;
    }
    if(dry_wet > 100){
        dry_wet = 100;
    }
    // change parameter
    x->wet = dry_wet/100.0;
    x->dry = 1.0 - x->wet;
}


/* the dsp-init-routine */
void hsd_chorus_dsp(t_hsd_chorus *x, t_signal **sp)
{
    /* check if samplerate has changed */
    if(x->sr != sp[0]->s_sr){
        
        
        t_int i;
        /* store the size of the delay-line. if the length of the delay-line needs to be changed, you need the new size and the old size */
        t_int oldbytes = x->delay_bytes;
        
        /* store the new sample rate */
        x->sr = sp[0]->s_sr;
        
        /* reallocate the delay-line. this process is similar to the one in the new-instance-routine except the function "resizebytes()" instead of "getbytes()" */
        t_int delay_line_length = (t_int)(x->sr * DELMAX/1000 + 1);
        x->delay_bytes = (int)(delay_line_length) * sizeof(t_float);
        x->delay_line_l = (t_float*)resizebytes((void*)x->delay_line_l,oldbytes, x->delay_bytes);
        if(x->delay_line_l == NULL){
            error("hsd_chorus~: cannot reallocate %ld bytes of memory", x->delay_bytes);
            return;
        }
        for (i=0; i<delay_line_length; i++) {
            x->delay_line_l[i] =0.0;
        }
        x->delay_line_r = (t_float*)resizebytes((void*)x->delay_line_r,oldbytes, x->delay_bytes);
        if(x->delay_line_r == NULL){
            error("hsd_chorus~: cannot reallocate %ld bytes of memory", x->delay_bytes);
            return;
        }
        for (i=0; i<delay_line_length; i++) {
            x->delay_line_r[i] =0.0;
        }
        x->delay_line_length = delay_line_length;
        x->write_index_l = 0;
        x->write_index_r = 0;
        
        // recalculate cycle_length
        x->cycle_length = x->sr/x->frequency;
        
    }
    
    /* add the objects signal processing to the signal-chain of puredata */
    dsp_add(hsd_chorus_perform,
            6,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[2]->s_vec,
            sp[3]->s_vec,
            sp[0]->s_n);
}


/* the perform routine */
t_int *hsd_chorus_perform(t_int *w)
{
    t_hsd_chorus *x = (t_hsd_chorus *) (w[1]);            //object data
    t_float *input_l = (t_float *) (w[2]);                //input-vector left
    t_float *input_r = (t_float *) (w[3]);                //input-vector right
    t_float *output_l = (t_float *) (w[4]);               //output-vector left
    t_float *output_r = (t_float *) (w[5]);               //output-vector right
    t_int n = w[6];                                       //buffer-size
    
    /* get needed data from data struct */
    t_float sr = x->sr;
    t_float *delay_line_l = x->delay_line_l;
    t_float *delay_line_r = x->delay_line_r;
    t_int read_index_l = x->read_index_l;
    t_int read_index_r = x->read_index_r;
    t_int write_index_l = x->write_index_l;
    t_int write_index_r = x->write_index_r;
    t_int delay_line_length = (int)(x->delay_line_length);
    t_float depth_l = x->depth_l;
    t_float depth_r = x->depth_r;
    t_float cycle_length = x->cycle_length;
    t_float phase = x->phase;
    t_float dry = x->dry;
    t_float wet = x->wet;
    /* init variables need for interpolation */
    
    // the next lower integer-value of the delay-length. -> the next accessable array-position
    t_int idelay_l, idelay_r;
    
    // the factor for the interpolation -> the offset between the real delay-length and the next smaller integer value of the delay-length
    t_float fraction_l, fraction_r;
    
    // the sample that is stored in the next lower accessable array-position
    t_float samp1_l, samp1_r;
    
    // the sample that is stored in the next higher accessable array-position
    t_float samp2_l, samp2_r;

    /* variable for storing the outputsample */
    t_float out_sample_l, out_sample_r;
    
    // running parameter for the LFO (runs from 0 to 1)
    t_float theta;
    
    // output of the LFO, sinewave from -1 to 1. for the right channel the same just with a phase shift
    t_float lfo_l, lfo_r;
    
    // delaylength after applying the modulation
    t_float delay_length_l, delay_length_r;
    
    //the second read index, used to read out the second sample used for interpolation
    t_int read_index2_l, read_index2_r;
    
    
    /* DSP-Loop */
    while (n--) {
        
        
        /* LFO */
        theta = phase / cycle_length;
        
        //calculate low frequency sinewave
        lfo_l = sin(2*PI*theta);
        lfo_r = sin(2*PI*theta + 0.5*PI); //second lfo with a 90° phase shift (90°=pi/2)
        
        //map values from (-1...+1) to (0...+1)
        lfo_l = (lfo_l + 1.0) / 2.0;
        lfo_r = (lfo_r + 1.0) / 2.0;
        
        // increase phase
        phase++;
        
        
        // reset phase after one period
        if (phase > cycle_length) {
            phase = 0;
        }
        
        // calculate delay by appling a sinusoidal modulation between 0 and depth ( 2 samples added for a minumum delay to avoid zero samples delay)
        delay_length_l = depth_l * lfo_l + 2;
        delay_length_r = depth_r * lfo_r + 2;
        
        /* delay line */
        
        // truncate the delay-length, so an array-entry can be accessed with it. (truncate -> round to next lower integer value)
        idelay_l = trunc(delay_length_l);
        idelay_r = trunc(delay_length_r);
        
        // calculate the factor for interpolation
        fraction_l = delay_length_l - idelay_l;
        fraction_r = delay_length_r - idelay_r;
        
        //set the offset between read & write; wrap it into legal space if needed
        read_index_l = write_index_l - idelay_l;
        read_index2_l = read_index_l - 1;
        while (read_index_l < 0) {
            read_index_l += delay_line_length;
        }
        while (read_index2_l < 0) {
            read_index2_l += delay_line_length;
        }
        
        
        read_index_r = write_index_r - idelay_r;
        read_index2_r = read_index_r - 1;
        while (read_index_r < 0) {
            read_index_r += delay_line_length;
        }
        while (read_index2_r < 0) {
            read_index2_r += delay_line_length;
        }
        
        // read the two samples for interpolation
        samp1_l = delay_line_l[read_index_l];
        samp2_l = delay_line_l[read_index2_l];
        samp1_r = delay_line_r[read_index_r];
        samp2_r = delay_line_r[read_index2_r];
        
        //quick buffer delayline-output before reading the input sample, so in case of shared input- and output-buffers, the input sample won´t be the recent written output-sample
        
        out_sample_l = samp1_l * fraction_l + samp2_l * (1.0-fraction_l);
        out_sample_r = samp1_r * fraction_r + samp2_r * (1.0-fraction_r);
        
        t_float input_left = *input_l++;
        t_float input_right = *input_r++;
        
        // write the input of the delay line
        delay_line_l[write_index_l++] = input_left ;
        delay_line_r[write_index_r++] = input_right ;
        
        //*output++ = out_sample + dry;
        *output_l++ = wet * out_sample_l + dry * input_left;
        *output_r++ = wet * out_sample_r + dry * input_right;
        
        
        //reset the write_index
        if(write_index_l >= delay_line_length){
            write_index_l -= delay_line_length;
        }
        if(write_index_r >= delay_line_length){
            write_index_r -= delay_line_length;
        }
    }
    x->write_index_l = write_index_l;
    x->write_index_r = write_index_r;
    x->phase = phase;
    
    return w+7;
}


/* free function that is called when the object is destroyed */
void hsd_chorus_free(t_hsd_chorus *x)
{
    freebytes(x->delay_line_l, x->delay_bytes);
    freebytes(x->delay_line_r, x->delay_bytes);

}




/* new-instance routine */
void *hsd_chorus_new(t_symbol *s, short argc, t_atom *argv)
{
    t_hsd_chorus *x = (t_hsd_chorus *)pd_new(hsd_chorus_class);

    
    // getting sample rate
    x->sr = sys_getsr();
    
    
    // initialise intermediate variables with default values
    t_float depth_ms_l = 10.0;
    t_float depth_ms_r = 10.0;
    t_float frequency = 1.0;
    t_float dry_wet = 50.0;
    
    /* get the creation arguments */
    if (argc>=4) {
        dry_wet= atom_getfloatarg(3, argc, argv);
    }
    if (argc>=3) {
        frequency = atom_getfloatarg(2, argc, argv);
    }
    if (argc>=2) {
        depth_ms_r = atom_getfloatarg(1, argc, argv);
    }
    if (argc>=1) {
        depth_ms_l = atom_getfloatarg(0, argc, argv);
    }

    // sanity checking
    if (depth_ms_l > DELMAX){
        depth_ms_l = DELMAX;
    }
    if (depth_ms_l < 0.0){
        depth_ms_l = 0.0;
    }
    if (depth_ms_r > DELMAX){
        depth_ms_r = DELMAX;
    }
    if (depth_ms_r < 0.0){
        depth_ms_r = 0.0;
    }
    if (frequency < 0.0) {
        frequency = 0.0;
    }
    if(dry_wet < 0){
        dry_wet = 0;
    }
    if(dry_wet > 100){
        dry_wet = 100;
    }
    
    
    // write the intermediate variables to the data struct
    x->depth_ms_l = depth_ms_l;
    x->depth_ms_r = depth_ms_r;
    x->frequency = frequency;
    x->wet = dry_wet/100.0;
    x->dry = 1.0 - x->wet;
    
    /* calculate depth & cycle_length from depth_ms and frequency */
    x->depth_l = x->sr * x->depth_ms_l/1000;
    x->depth_r = x->sr * x->depth_ms_r/1000;
    x->cycle_length = x->sr / x->frequency;
    
    //creating the second signal inlet (the first one is generated automatically by "CLASS_MAINSIGNALIN")
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));
    
    // creating the active inlets. the function specified in the last argument is called, when the inlet receives a float message
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("depth_l"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("depth_r"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("frequency"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("drywet"));
    
    //creating the signal-outlets
    outlet_new(&x->obj, gensym("signal"));
    outlet_new(&x->obj, gensym("signal"));
    
    //Allocating the DelayLines
    t_int delay_line_length = (t_int)(x->sr * DELMAX/1000 + 1);
    x->delay_bytes = delay_line_length * sizeof(t_float);
    x->delay_line_l = (t_float*)getbytes(x->delay_bytes);
    if(x->delay_line_l == NULL){
        error("hsd_chorus~: cannot allocate %ld bytes of memory", x->delay_bytes);
        return NULL;
    }
    t_int i;
    for (i=0; i<delay_line_length; i++) {
        x->delay_line_l[i] =0.0;
    }
    x->delay_line_r = (t_float*)getbytes(x->delay_bytes);
    if(x->delay_line_r == NULL){
        error("hsd_chorus~: cannot allocate %ld bytes of memory", x->delay_bytes);
        return NULL;
    }
    for (i=0; i<delay_line_length; i++) {
        x->delay_line_r[i] =0.0;
    }
    
    x->delay_line_length = delay_line_length;
    x->write_index_l = 0;
    x->write_index_r = 0;
    x->phase = 0;
    return x;
}

/* setup routine */
void hsd_chorus_tilde_setup(void){
    
    hsd_chorus_class = class_new(gensym("hsd_chorus~"),
                               (t_newmethod)hsd_chorus_new,
                               (t_method)hsd_chorus_free,
                               sizeof(t_hsd_chorus),
                               0,
                               A_GIMME,
                               0);
    
    CLASS_MAINSIGNALIN(hsd_chorus_class, t_hsd_chorus, x_f);
    
    
    class_addmethod(hsd_chorus_class,
                    (t_method)hsd_chorus_dsp,
                    gensym("dsp"),
                    0);
    class_addmethod(hsd_chorus_class,
                    (t_method)hsd_chorus_depth_l,
                    gensym("depth_l"),
                    A_DEFFLOAT,
                    0);
    class_addmethod(hsd_chorus_class,
                    (t_method)hsd_chorus_depth_r,
                    gensym("depth_r"),
                    A_DEFFLOAT,
                    0);
    class_addmethod(hsd_chorus_class,
                    (t_method)hsd_chorus_frequency,
                    gensym("frequency"),
                    A_DEFFLOAT,
                    0);
    class_addmethod(hsd_chorus_class,
                    (t_method)hsd_chorus_drywet,
                    gensym("drywet"),
                    A_DEFFLOAT,
                    0);
    
    
    post ("hsd_chorus~ by David Bau, HS Duesseldorf ");
    
}