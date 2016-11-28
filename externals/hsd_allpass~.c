/* hsd_allpass~ external from the HSD-Library, University of Applied Science Duesseldorf
 Created by David Bau, 27.03.2015
 
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
 

 This is an allpass filter, derived from Will Pirkles "Designing Audio Effect Plug-Ins in C++"
    - a forward delay-line, which delays the signal by "D" samples
    - a feedback-path, the input of the delay-line is always the current input sample (x(n)) and the current delay-line-output-sample (yDL) multiplied by a feedback-factor "g"
        -> FB = g * yDL
        -> xDL = x(n) + FB
 
    - a feedforward-path, where the input of the delay-line is directly forwarded to the output without delay
        -> multiplied by inverted gain
        -> FF = -g * xDL
 
        => y(n) = FF + yDL
 
             -------------(*g)<---------
            |      FB                   |
            |                           |
            |                           |
     —>x    v        ________________   |           y—>
     o—————(+)—-o——>|______z-D_______|—-o-—(+)---———>o
                |                           ^
                |                           |
                |                           |
                |                     FF    |
                 --------->(*-g1)-----------
 

 This external is very similar to the hsd_comb~-external. The difference is that there is an additional feed forward path around the delay-line (with inverted gain). This creates an allpass filter.
 Thus, the code is also very similar, except for a slightly different difference equation and the renaming of the coefficient from "feedback"(comb) to "gain"(allpass).
 
 */

#include "m_pd.h"
#include "math.h"

/* defaults */
#define DELMAX 100

/* data struct */
typedef struct _hsd_allpass{
    
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
    
    /* the paramter that is set from outside and indicates the time the audio-signal is delayed. this value needs to be converted to the amount of samples needed for delay_length (NOT delay-line length) to determine the displacement between read- and write pointer*/
    t_float delay_time_ms;
    
    /* this is the real amount of delay. it determines the displacement between the read- and writepointer and therefore the amount of delayed samples. note that this is a float value, because some millisecond-values can result in noninteger sample-values. in this case an interpolation is necessary (as done in the perform routine) */
    t_float delay_length;
    
    /* the current position of the write-pointer. it is incremented with every sample-tick in the dsp-loop. it indicates the position, where the input is written to the delay-line. */
    t_int write_index;
    
    /* the current position of the read-pointer. it "follows" the write pointer skimming thorugh the delay-line with a displacement of delay_length. this displacement is achieved by subtracting the delay_length from the write-pointer */
    t_int read_index;

    /* this value determines, how much of the output signal is fed back into the delay line again. it represents the factor "g" from the schematic in the description above */
    t_float g;
    
    /* dummy float for CLASS_MAINSIGNALIN */
    t_float x_f;
    
}t_hsd_allpass;

static t_class *hsd_allpass_class;

/* function prototypes */
void *hsd_allpass_new (t_symbol *s, short argc, t_atom *argv);
void hsd_allpass_dsp(t_hsd_allpass *x, t_signal **sp);
t_int *hsd_allpass_perform(t_int *w);
void hsd_allpass_free(t_hsd_allpass *x);
void hsd_allpass_delaytime(t_hsd_allpass *x, t_floatarg f);
void hsd_allpass_gain(t_hsd_allpass *x, t_floatarg f);
void hsd_allpass_bang(t_hsd_allpass *x);



/* function for setting the delay time in ms. called when a float is received by the second inlet */
void hsd_allpass_delaytime(t_hsd_allpass *x, t_floatarg f){
    
    t_float delay_time_ms = f;
    
    // sanity checking
    if(delay_time_ms > DELMAX || delay_time_ms <=0.0){
        error("hsd_allpass~: illegal delay time: %f. delay time set to 10ms", delay_time_ms);
        delay_time_ms=10.0;
    }
    // calculating the needed samples from the millisecond-value
    x->delay_length = x->sr * delay_time_ms/1000;
    x->delay_time_ms = delay_time_ms;
    
}

/* function for setting the delay time in ms. called when a float is received by the second inlet */
void hsd_allpass_gain(t_hsd_allpass *x, t_floatarg f){
    
    t_float g = f;
    
    // sanity checking
    if (g > 1 || g < 0) {
        error("illegal g: %f. g set to 0", g);
        g = 1;
    }
    x->g = g;

}

/* function for clearing the delay-line */
void hsd_allpass_bang(t_hsd_allpass *x){
    
    int i;
    // set all values of the delay line to zero
    for (i=0; i<x->delay_line_length; i++) {
        x->delay_line[i] =0.0;
    }
    // reset the write pointer
    x->write_index = 0;
    
}

// DSP-init routine
void hsd_allpass_dsp(t_hsd_allpass *x, t_signal **sp)
{
    
    
    /* check for sample rate change and recalculate the delay-line, if necessary */
    if(x->sr != sp[0]->s_sr){
        
        int i;
        
        /* store the size of the delay-line. if the length of the delay-line needs to be changed, you need the new size and the old size */
        int oldbytes = x->delay_bytes;
        
        /* store the new sample rate */
        x->sr = sp[0]->s_sr;
        
        /* reallocate the delay-line. this process is similar to the one in the new-instance-routine except the function "resizebytes()" instead of "getbytes()" */
        int delay_line_length = (int)(x->sr * DELMAX/1000 + 1);
        x->delay_bytes = (int)(delay_line_length * sizeof(t_float));
        x->delay_line = (t_float*)resizebytes((void*)x->delay_line,oldbytes, x->delay_bytes);
        if(x->delay_line == NULL){
            error("hsd_allpass~: cannot reallocate %ld bytes of memory", x->delay_bytes);
            return;
        }
        for (i=0; i<delay_line_length; i++) {
            x->delay_line[i] =0.0;
        }
        x->delay_line_length = delay_line_length;
        
        //renew the offset between read and write pointer
        x->delay_length = x->sr * x->delay_time_ms/1000 + 1;
        
        x->write_index = 0;
    }
    
    /* add the objects signal processing to the signal-chain of puredata */
    dsp_add(hsd_allpass_perform,
            4,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[0]->s_n);
    
}


/* the perform routine */
t_int *hsd_allpass_perform(t_int *w)
{
    /* get the signal vectors */
    t_hsd_allpass *x = (t_hsd_allpass *) (w[1]);              //object data
    t_float *input = (t_float *) (w[2]);                   //input-vector
    t_float *output = (t_float *) (w[3]);                  //output-vector
    t_int n = w[4];                                     //buffer-size
    
    /* get needed data from data struct */
    t_float *delay_line = x->delay_line;
    t_int read_index = x->read_index;
    t_int write_index = x->write_index;
    t_float delay_length = x->delay_length;
    t_float g = x->g;
    
    
    /* init variables for the interpolation */
    
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
    
    /* variable for storing the input of the delay line */
    t_float xDL;
    
    /* DSP-Loop */
    while (n--) {
        
        // truncate the delay-length, so an array-entry can be accessed with it. (truncate -> round to next lower integer value)
        idelay = trunc(delay_length);
        
        // calculate the factor for interpolation
        fraction = delay_length - idelay;
        
        //set the offset between read & write-pointer. wrap it into legal space if needed
        read_index = write_index - idelay;
        while (read_index < 0) {
            read_index += delay_length;
        }
        
        // read the two samples
        samp1 = delay_line[read_index];
        samp2 = delay_line[(read_index + 1) % idelay]; //(if the read_index is at the end of the array, (read_index+1) has to be wrapped into legal space (beginning of the array) again )
        
        //quick buffer delayline-output before reading the input sample, so in case of shared input- and output-buffers, the input sample won´t be the recent written output-sample
        out_sample = samp1 + fraction* (samp2- samp1);
        
        // input of the delay line --> x(n) + g*y(n)
        xDL = *input++ + out_sample * g;
        
        // write the input of the delay line
        delay_line[write_index++] = xDL;
        
        //output y(n) = yDL + FF
        *output++ = out_sample + (-g * xDL);
        
        //reset the write_index
        if(write_index >= delay_length){
            write_index -= delay_length;
        }
        
    }
    
    x->write_index = write_index;
    
  
    return w+5;
}



/* free function that is called when the object is destroyed */
void hsd_allpass_free(t_hsd_allpass *x)
{
    freebytes(x->delay_line, x->delay_bytes);
}



/* new-instance routine */
void *hsd_allpass_new(t_symbol *s, short argc, t_atom *argv)
{
    // set default values
    t_float g=0.1;
    t_float delay_time_ms=30;
    
    t_hsd_allpass *x = (t_hsd_allpass *)pd_new(hsd_allpass_class);
    
    // creating the two active inlets. the functions specified in the last argument are called, when the inlet receives a message
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("delaytime"));
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("gain"));
    
    
    //creating the signal-outlet
    outlet_new(&x->obj, gensym("signal"));
    
    // getting sample rate
    x->sr = sys_getsr();
    
    
    /* getting creation arguments */
    if (argc>=2) {
        g  = atom_getfloatarg(1, argc, argv);
    }
    if (argc>=1) {
        delay_time_ms = atom_getfloatarg(0, argc, argv);
    }
    
    if (g > 1 || g < 0) {
        error("illegal g: %f. g set to 1", g);
        g = 1;
    }
    
    if(delay_time_ms > DELMAX || delay_time_ms<=0.0){
        error("hsd_allpass~: illegal delay time: %f. delay time set to 10ms", delay_time_ms);
        delay_time_ms=10.0;
    }
    x->delay_time_ms = delay_time_ms;
    x->delay_length = x->sr * delay_time_ms/1000 + 1;
    
    
    /* Allocating the DelayLine */
    int delay_line_length = (int)(x->sr * DELMAX/1000 + 1);
    x->delay_bytes = (int)(delay_line_length * sizeof(t_float));
    x->delay_line = (t_float*)getbytes(x->delay_bytes);
    if(x->delay_line == NULL){
        error("hsd_allpass~: cannot allocate %d bytes of memory", x->delay_bytes);
        return NULL;
    }
    int i;
    for (i=0; i<delay_line_length; i++) {
        x->delay_line[i] =0.0;
    }
    
    x->delay_line_length = delay_line_length;
    x->g = g;
    x->write_index=0;
    
    return x;
}

/* setup routine */
void hsd_allpass_tilde_setup(void){
    
    hsd_allpass_class = class_new(gensym("hsd_allpass~"),
                               (t_newmethod)hsd_allpass_new,
                               (t_method)hsd_allpass_free,
                               sizeof(t_hsd_allpass),
                               0,
                               A_GIMME,
                               0);
    
    CLASS_MAINSIGNALIN(hsd_allpass_class, t_hsd_allpass, x_f);
    
    
    class_addmethod(hsd_allpass_class,
                    (t_method)hsd_allpass_dsp,
                    gensym("dsp"),
                    0);
    
    class_addmethod(hsd_allpass_class,
                    (t_method)hsd_allpass_delaytime,
                    gensym("delaytime"),
                    A_DEFFLOAT,
                    0);
    class_addmethod(hsd_allpass_class,
                    (t_method)hsd_allpass_gain,
                    gensym("gain"),
                    A_DEFFLOAT,
                    0);
    class_addbang(hsd_allpass_class, hsd_allpass_bang);
    
    
    post ("hsd_allpass~ by David Bau, University of Applied Sciences Duessldorf");
    
}