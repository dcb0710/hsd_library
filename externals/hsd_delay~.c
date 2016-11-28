/* hsd_delay~ external from the HSD-Library, University of Applied Science Duesseldorf
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
 

 This is a basic delay-line, implemented with a ring-buffer. It constantly writes the incoming samples to an array (with a samplewise increasing write-pointer). Simultaneously, a read-pointer reads out the samples written to the array before. The offset between the write pointer and the read pointer is determined by the delay-time. When the pointers reach the end of the array, they are "wrapped" to the start of the array, thus the pointer is set to the value 0.
 
 
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
 
 
 To allocate memory for a huge array as it is used here, the function "getbytes()" from m_pd.h is used. It reserves a certain amount of bytes and returns a pointer to the start of the array. This pointer is stored in the *delay_line and can be accessed like a regular array. It is important to keep track of the size of the array.
    -> for resizing: the length of the delay line depends on the sample rate. So everytime the sample-rate of pd changes, the delay line need to have another length and has to be resized. This is done by the function "resizebytes()", which needs the the new AND the old number of bytes
    -> for deallocating: it is very important to free the memory allocated with "getbytes()" at the end of runtime, because puredata won´t do it by itself. for this purpose, a free-function has to be defined (hsd_delay_free()). This function is passed with the "class-new"-function call. It is called when puredata shuts down or the object is deleted. Here you can call the function "freebytes()"
 
 The offset of the pointers is calculated with the delay-time in ms (-> sr*delay_ms / 1000). If the offset is a noninteger value, the read-pointer has to read the next possible two integer values and interpolate between them to generate the output sample.
 
 
 The delay-line introduced here is used in other hsd-externals and can be used for further development.
 
 
 
 */

#include "m_pd.h"
#include "math.h"

/* defaults */
#define DELMAX 100
#define DEFAULT_TIME 10  //10ms

/* data struct */
typedef struct _hsd_delay{
    
    /* the object data itself */
    t_object obj;
    
    /* sample rate */
    t_float sr;
    
    /* the length of the complete delay-line in samples. the maximum delay time is defined by DELMAX 100 milliseconds, therefore the delayline is always allocated with enough samples to store 100ms of audio. this value is always in dependance of the samplerate and has to be recalculated when the sample rate changes */
    t_float delay_line_length;
    
    /* the length of the delay-line in bytes. it is proportional to delay_line_length, which is just mulitplied by the byte-size of a float variable. it is important to store the bytes, because when the object is destroyed at the end of runtime, the allocated memory has to be freed again */
    t_float delay_bytes;
    
    /* the pointer to the delay-line itself. when the memory of the delay-line is allocated, a pointer to this memory (to the first entry) is returned and stored in this variable. */
    t_float *delay_line;
    
    /* the paramter that is set from outside and indicates the time the audio-signal is delayed. this value needs to be converted to the amount of samples needed for the delay_length (NOT delay-line length) to determine the displacement between read- and write pointer*/
    t_float delay_time_ms;
    t_float delay_length;
    
    /* the current position of the write-pointer. it is incremented with every sample-tick in the dsp-loop. it indicates the position, where the input is written to the delay-line. */
    t_int write_index;
    
    /* the current position of the read-pointer. it "follows" the write pointer skimming thorugh the delay-line with a displacement of delay_length. this displacement is achieved by subtracting the delay_length from the write-pointer */
    t_int read_index;
    
    /* dummy float for CLASS_MAINSIGNALIN */
    t_float x_f;
    
    
}t_hsd_delay;

static t_class *hsd_delay_class;


/* function prototypes */
void *hsd_delay_new (t_floatarg f);
void hsd_delay_dsp(t_hsd_delay *x, t_signal **sp);
t_int *hsd_delay_perform(t_int *w);
void hsd_delay_free(t_hsd_delay *x);
void hsd_delay_delaytime(t_hsd_delay *x, t_floatarg f);
void hsd_delay_bang(t_hsd_delay *x);



/* function for setting the delay time, called by the second inlet, perofmrs sanity checking */
void hsd_delay_delaytime(t_hsd_delay *x, t_floatarg f){
    
    t_float delay_time_ms = f;
    
    // sanity checking
    if(delay_time_ms > DELMAX || delay_time_ms <=0.0){
        error("hsd_delay~: illegal delay time: %f. delay time set to 10ms", delay_time_ms);
        delay_time_ms=10.0;
    }
    // calculate the delay length in bytes
    x->delay_length = x->sr * delay_time_ms/1000;
    x->delay_time_ms = delay_time_ms;
    
}

/* function for resetting the delay line, executed when a bang message is received by any inlet */
void hsd_delay_bang(t_hsd_delay *x){
    
    t_int i;
    // set all values of the delay line to zero
    for (i=0; i<x->delay_line_length; i++) {
        x->delay_line[i] =0.0;
    }
    // reset the write pointer
    x->write_index = 0;
    
}

/* the dsp-init-routine */
void hsd_delay_dsp(t_hsd_delay *x, t_signal **sp)
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
        x->delay_bytes = delay_line_length * sizeof(t_float);
        x->delay_line = (t_float*)resizebytes((void*)x->delay_line,oldbytes, x->delay_bytes);
        if(x->delay_line == NULL){
            error("hsd_delay~: cannot reallocate %f bytes of memory", x->delay_bytes);
            return;
        }
        for (i=0; i<delay_line_length; i++) {
            x->delay_line[i] =0.0;
        }
        x->delay_line_length = delay_line_length;
        x->write_index = 0;
        
        //renew the offset between read and write pointer
        x->delay_length = x->sr * x->delay_time_ms/1000 + 1;
        
    }
    
    /* add the objects signal processing to the signal-chain of puredata */
    dsp_add(hsd_delay_perform,
            4,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[0]->s_n);
}


/* the perform routine */
t_int *hsd_delay_perform(t_int *w)
{
    t_hsd_delay *x = (t_hsd_delay *) (w[1]);            //object data
    t_float *input = (t_float *) (w[2]);                //input-vector
    t_float *output = (t_float *) (w[3]);               //output-vector
    t_int n = w[4];                                     //buffer-size
    
    /* get needed data from data struct */
    t_float *delay_line = x->delay_line;
    t_int read_index = x->read_index;
    t_int write_index = x->write_index;
    t_int delay_length = x->delay_length;
    t_int delay_line_length = (int)(x->delay_line_length);
    
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
    
    //the second read index, used to read out the second sample used for interpolation
    t_int read_index2;
    
    
    /* DSP-Loop */
    while (n--) {
        
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
        delay_line[write_index++] = *input++;
        
        *output++ = out_sample;
        
        //reset the write_index
        if(write_index >= delay_line_length){
            write_index -= delay_line_length;
        }
    }
    x->write_index = write_index;
    
    return w+5;
}


/* free function that is called when the object is destroyed */
void hsd_delay_free(t_hsd_delay *x)
{
    freebytes(x->delay_line, x->delay_bytes);
}




/* new-instance routine */
void *hsd_delay_new(t_floatarg f)
{
    // set the initial delay time either by default or creation argument
    t_float delay_time_ms;
    if (f) {
        delay_time_ms=f;
    }else{
        delay_time_ms = DEFAULT_TIME;
    }
    
    t_hsd_delay *x = (t_hsd_delay *)pd_new(hsd_delay_class);
    
    // creating the active inlet. the function specified in the last argument is called, when the inlet receives a message
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("delaytime"));    
    //creating the signal-outlet
    outlet_new(&x->obj, gensym("signal"));
    
    // getting sample rate
    x->sr = sys_getsr();
    
    if(delay_time_ms > DELMAX || delay_time_ms<=0.0){
        error("hsd_delay~: illegal delay time: %f. delay time set to 10ms", delay_time_ms);
        delay_time_ms=10.0;
    }
    x->delay_time_ms = delay_time_ms;
    x->delay_length = x->sr * delay_time_ms/1000 + 1;
    
    
    //Allocating the DelayLine
    t_int delay_line_length = (t_int)(x->sr * DELMAX/1000 + 1);
    x->delay_bytes = delay_line_length * sizeof(t_float);
    x->delay_line = (t_float*)getbytes(x->delay_bytes);
    if(x->delay_line == NULL){
        error("hsd_delay~: cannot allocate %f bytes of memory", x->delay_bytes);
        return NULL;
    }
    t_int i;
    for (i=0; i<delay_line_length; i++) {
        x->delay_line[i] =0.0;
    }
    
    x->delay_line_length = delay_line_length;
    x->write_index=0;
    
    return x;
}

/* setup routine */
void hsd_delay_tilde_setup(void){
    
    hsd_delay_class = class_new(gensym("hsd_delay~"),
                               (t_newmethod)hsd_delay_new,
                               (t_method)hsd_delay_free,
                               sizeof(t_hsd_delay),
                               0,
                               A_DEFFLOAT,
                               0);
    
    CLASS_MAINSIGNALIN(hsd_delay_class, t_hsd_delay, x_f);
    
    
    class_addmethod(hsd_delay_class,
                    (t_method)hsd_delay_dsp,
                    gensym("dsp"),
                    0);
    
    class_addmethod(hsd_delay_class,
                    (t_method)hsd_delay_delaytime,
                    gensym("delaytime"),
                    A_DEFFLOAT,
                    0);

    class_addbang(hsd_delay_class, hsd_delay_bang);
    
    
    post ("hsd_delay~ by David Bau, HS Duesseldorf ");
    
}