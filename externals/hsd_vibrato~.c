/* hsd_vibrato~ external from the HSD-Library, University of Applied Science Duesseldorf
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
 
 
 This is a vibrato / flanger effect. By modulating the length of a delay, the pitch of the sound goes up and down when the output is read faster or slower from the delay line, because of the doppler effect. It is basicylly the same as playing a tape recorder at variable speed.
 It consists of a delay-line (see hsd_variabledelay~), where the offset of the readpointer is modulated by an Sinewave-LFO. The delay will vary between zero and the adjusted effect "depth". 
 If the hsd_vibrato~ is combined with a dry signal in PD, the flanging effect occurs.
 To increase the "drama" of the flanging-effect, you can set a feedback amount from the output of the delay-line to the input. Applying the feedback to a plain vibrato, main sound interesting, but quite unuseful.
 
 
        ___________________________________________________________________
        |               delay-line                                         |
        |                                                                  |
        |                    (array)                                       |
        |__|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_|_......|_|_|
                    ^ ->                        ^ ->
                    |                           |
                    | read-pointer              | write-pointer
                    |                           |
                    |  <- - - - - - - - - - ->  |
                    v            offset         ^
                 (output)                    (input)
 
               < - - - - >
                   LFO
 
 
                                 _
                                 /|
                                /
        —>x            ________/___________         y—>
        o—————-(+)———>|______z-D___________|———————>o
                ^            /                  |
                |           /                   |
                |          /                    |
                |                               |
                |                               |
                 ------------(*fb)<-------------
 
            D = between 0 and "depth", sinusoidal modulated
 
 
 */

#include "m_pd.h"
#include "math.h"

/* defaults */
#define DELMAX 20

#define PI 3.1415926536

/* data struct */
typedef struct _hsd_vibrato{
    
    /* the object data itself */
    t_object obj;
    
    /* sample rate */
    t_float sr;
    
    /* the length of the complete delay-line in samples. the maximum delay time is defined by DELMAX 100 milliseconds, therefore the delayline is always allocated with enough samples to store 100ms of audio. this value is always in dependance of the samplerate and has to be recalculated when the sample rate changes */
    t_float delay_line_length;
    
    /* the length of the delay-line in bytes. it is proportional to delay_line_length, which is just mulitplied by the byte-size of a float variable. it is important to store the bytes, because when the object is destroyed at the end of runtime, the allocated memory has to be freed again */
    t_int delay_bytes;
    
    /* the pointer to the delay-line itself. when the memory of the delay-line is allocated, a pointer to this memory (to the first entry) is returned and stored in this variable. */
    t_float *delay_line;
    
    /* the current position of the write-pointer. it is incremented with every sample-tick in the dsp-loop. it indicates the position, where the input is written to the delay-line. */
    t_int write_index;
    
    /* the current position of the read-pointer. it "follows" the write pointer skimming thorugh the delay-line with a displacement of delay_length. this displacement is achieved by subtracting the delay_length from the write-pointer */
    t_int read_index;
    
    /* dummy float for CLASS_MAINSIGNALIN */
    t_float x_f;
    
    /* modulation of the delay_time in samples and milliseconds */
    t_float depth;
    t_float depth_ms;
    
    /* frequency of the LFO */
    t_float frequency;
    
    /* time interval in samples of the LFO */
    t_float cycle_length;
    
    /* oscillator phase. it runs from 0 up to the cycle-length and is incremented every sample tick by 1.0 */
    t_float phase;
    
    /* z element for the allpass interpolation */
    t_float z_alp;
    
    /* determines the amount of the DDL-Output that is sent back to its input. Range between -0.99 and 0.99 */
    t_float feedback;
    
    
}t_hsd_vibrato;

static t_class *hsd_vibrato_class;


/* function prototypes */
void *hsd_vibrato_new(t_floatarg f1, t_floatarg f2, t_floatarg f3);
void hsd_vibrato_dsp(t_hsd_vibrato *x, t_signal **sp);
t_int *hsd_vibrato_perform(t_int *w);
void hsd_vibrato_free(t_hsd_vibrato *x);
void hsd_vibrato_depth(t_hsd_vibrato *x, t_floatarg f);
void hsd_vibrato_frequency(t_hsd_vibrato *x, t_floatarg f);
void hsd_vibrato_feedback(t_hsd_vibrato *x, t_floatarg f);


/* function for setting the modulation depth, called by the third inlet, performs sanity checking */
void hsd_vibrato_depth(t_hsd_vibrato *x, t_floatarg f){
    
    t_float depth_ms = f;
    
    // sanity checking
    if(depth_ms > DELMAX){
        depth_ms=DELMAX;
        
    }
    if (depth_ms < 0) {
        depth_ms = 0;
    }
    x->depth_ms = depth_ms;
    x->depth = x->sr * x->depth_ms/1000;
    
    
}

void hsd_vibrato_frequency(t_hsd_vibrato *x, t_floatarg f){
    
    t_float frequency = f;
    
    // sanity checking
    if(frequency <= 0){
        error("hsd_vibrato~: frequency must be nonzero & positive");
    }else{
        x->cycle_length = x->sr/frequency;
        x->frequency = frequency;
    }
    
}

void hsd_vibrato_feedback(t_hsd_vibrato *x, t_floatarg f){
    
    t_float feedback = f;
    
    // sanity checking
    if(feedback > 0.99){
        feedback = 0.99;
        
    }
    if(feedback < -0.99){
        feedback = -0.99;
        
    }
    
    x->feedback = feedback;
    
    
    
}


/* the dsp-init-routine */
void hsd_vibrato_dsp(t_hsd_vibrato *x, t_signal **sp)
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
        x->delay_line = (t_float*)resizebytes((void*)x->delay_line,oldbytes, x->delay_bytes);
        if(x->delay_line == NULL){
            error("hsd_vibrato~: cannot reallocate %ld bytes of memory", x->delay_bytes);
            return;
        }
        for (i=0; i<delay_line_length; i++) {
            x->delay_line[i] =0.0;
        }
        x->delay_line_length = delay_line_length;
        x->write_index = 0;
        
        // recalculate cycle_length
        x->cycle_length = x->sr/x->frequency;
        
    }
    
    /* add the objects signal processing to the signal-chain of puredata */
    dsp_add(hsd_vibrato_perform,
            4,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[0]->s_n);
}


/* the perform routine */
t_int *hsd_vibrato_perform(t_int *w)
{
    t_hsd_vibrato *x = (t_hsd_vibrato *) (w[1]);            //object data
    t_float *input = (t_float *) (w[2]);                //input-vector
    t_float *output = (t_float *) (w[3]);               //output-vector
    t_int n = w[4];                                     //buffer-size
    
    /* get needed data from data struct */
    t_float *delay_line = x->delay_line;
    t_int read_index = x->read_index;
    t_int write_index = x->write_index;
    t_int delay_line_length = (int)(x->delay_line_length);
    t_float depth = x->depth;
    t_float cycle_length = x->cycle_length;
    t_float phase = x->phase;
    t_float feedback = x->feedback;
    
    /* init variables need for interpolation */
    
    // the next lower integer-value of the delay-length. -> the next accessable array-position
    t_int idelay;
    
    // the factor for the interpolation -> the offset between the real delay-length and the next smaller integer value of the delay-length
    t_float fraction;
    
    // the sample that is stored in the next lower accessable array-position
    t_float samp1;
    
    // the sample that is stored in the next higher accessable array-position
    t_float samp2;

    /* variable for storing the outputsample */
    t_float out_sample;
    
    // running parameter for the LFO (runs from 0 to 1)
    t_float theta;
    
    // output of the LFO, sinewave from -1 to 1
    t_float lfo;
    
    // delaylength after applying the modulation
    t_float delay_length;
    
    //the second read index, used to read out the second sample used for interpolation
    t_int read_index2;
    
    /* DSP-Loop */
    while (n--) {
        
        
        /* LFO */
        theta = phase / cycle_length;
        
        //calculate low frequency sinewave
        lfo = sin(2*PI*theta);
        
        //map values from (-1...+1) to (0...+1)
        lfo = (lfo + 1.0) / 2.0;
        
        // increase phase
        phase++;
        
        
        // reset phase after one period
        if (phase > cycle_length) {
            phase = 0;
        }
        
        // calculate delay by appling a sinusoidal modulation between 0 and depth ( 2 samples added for a minumum delay to avoid zero samples delay)
        delay_length = depth * lfo + 2;
        
        /* delay line */
        
        // truncate the delay-length, so an array-entry can be accessed with it. (truncate -> round to next lower integer value)
        idelay = trunc(delay_length);
        
        // calculate the factor for interpolation
        fraction = delay_length - idelay;
        
        //set the offset between read & write; wrap it into legal space if needed
        read_index = write_index - idelay;
        read_index2 = read_index - 1;
        while (read_index < 0) {
            read_index += delay_line_length;
        }
        while (read_index2 < 0) {
            read_index2 += delay_line_length;
        }
        
        
        // read the two samples for interpolation
        samp1 = delay_line[read_index];
        samp2 = delay_line[read_index2];
        
        //quick buffer delayline-output before reading the input sample, so in case of shared input- and output-buffers, the input sample won´t be the recent written output-sample
        
        out_sample = samp1 * fraction + samp2 * (1.0-fraction);

        
        
        
        // write the input of the delay line
        delay_line[write_index++] = *input++ + out_sample*feedback;
        
        *output++ = out_sample;
        
        //reset the write_index
        if(write_index >= delay_line_length){
            write_index -= delay_line_length;
        }
    }
    x->write_index = write_index;
    x->phase = phase;
    
    return w+5;
}


/* free function that is called when the object is destroyed */
void hsd_vibrato_free(t_hsd_vibrato *x)
{
    freebytes(x->delay_line, x->delay_bytes);
}




/* new-instance routine */
void *hsd_vibrato_new(t_floatarg f1, t_floatarg f2, t_floatarg f3)
{
    t_hsd_vibrato *x = (t_hsd_vibrato *)pd_new(hsd_vibrato_class);

    
    // getting sample rate
    x->sr = sys_getsr();
    
    /* getting creation arguments */
    
    // initialise intermediate variables
    t_float depth_ms, frequency, feedback;
    
    // check if arguments have been passed by the user and write it to the intermediate variables. if the argument was not passed, the default value is applied
    if(f1){
        depth_ms = f1;
    }else{
        depth_ms = 0.0; //default no modulation
    }
    
    if (f2){
        frequency = f2;
    }else{
        frequency = 1.0; //default 1 Hz
    }
    
    if (f3){
        feedback = f3;
    }else{
        feedback = 0; //default no feedback
    }
    
    // sanity checking
    if (depth_ms > DELMAX){
        depth_ms = DELMAX;
    }
    if (depth_ms < 0.0){
        depth_ms = 0.0;
    }
    if (frequency < 0.0) {
        frequency = 0.0;
    }
    if (feedback > 0.99) {
        feedback = 0.99;
    }
    if (feedback < -0.99) {
        feedback = 0.99;
    }
    
    // write the intermediate variables to the data struct
    x->depth_ms = depth_ms;
    x->frequency = frequency;
    x->feedback = feedback;
    
    /* calculate depth & cycle_length from depth_ms and frequency */
    x->depth = x->sr * x->depth_ms/1000;
    x->cycle_length = x->sr / x->frequency;
    
    
    // creating the active inlets. the function specified in the last argument is called, when the inlet receives a float message
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("depth"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("frequency"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("feedback"));
    
    
    //creating the signal-outlet
    outlet_new(&x->obj, gensym("signal"));
    
    
    //Allocating the DelayLine
    t_int delay_line_length = (t_int)(x->sr * DELMAX/1000 + 1);
    x->delay_bytes = delay_line_length * sizeof(t_float);
    x->delay_line = (t_float*)getbytes(x->delay_bytes);
    if(x->delay_line == NULL){
        error("hsd_vibrato~: cannot allocate %ld bytes of memory", x->delay_bytes);
        return NULL;
    }
    t_int i;
    for (i=0; i<delay_line_length; i++) {
        x->delay_line[i] =0.0;
    }
    
    x->delay_line_length = delay_line_length;
    x->write_index=0;
    x->phase = 0;
    return x;
}

/* setup routine */
void hsd_vibrato_tilde_setup(void){
    
    hsd_vibrato_class = class_new(gensym("hsd_vibrato~"),
                               (t_newmethod)hsd_vibrato_new,
                               (t_method)hsd_vibrato_free,
                               sizeof(t_hsd_vibrato),
                               0,
                               A_DEFFLOAT,
                               A_DEFFLOAT,
                               A_DEFFLOAT,
                               0);
    
    CLASS_MAINSIGNALIN(hsd_vibrato_class, t_hsd_vibrato, x_f);
    
    
    class_addmethod(hsd_vibrato_class,
                    (t_method)hsd_vibrato_dsp,
                    gensym("dsp"),
                    0);
    class_addmethod(hsd_vibrato_class,
                    (t_method)hsd_vibrato_depth,
                    gensym("depth"),
                    A_DEFFLOAT,
                    0);
    class_addmethod(hsd_vibrato_class,
                    (t_method)hsd_vibrato_frequency,
                    gensym("frequency"),
                    A_DEFFLOAT,
                    0);
    class_addmethod(hsd_vibrato_class,
                    (t_method)hsd_vibrato_feedback,
                    gensym("feedback"),
                    A_DEFFLOAT,
                    0);
    
    
    post ("hsd_vibrato~ by David Bau, HS Duesseldorf ");
    
}