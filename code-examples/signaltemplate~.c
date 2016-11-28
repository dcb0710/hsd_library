/* signaltemplate~.c
 Created by David Bau, 27.08.2015
 
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

 This is a more detailed demonstration of an external. In contrast to the helloworld external, it is much bigger and has a lot more of comments & explanations. This is might serve both as a starting point (=template) for new DSP-Externals, and as some sort of reference work
 
 For an shorter, uncommented version, see "signaltemplate~_unceommented.c"
 
 */

#include "m_pd.h"

/* this is a pointer to the class for "signaltemplate", which is created in the
 "setup" routine below and used to create new ones in the "new" routine. */
static t_class *signaltemplate_class;


//=====================================================================================


/* The data structure: This is a struct that contains all variables that are needed for running the object. It is initialised for every new instance of the external in the "signaltemplate_new"-function (with pd_new() ).  */
typedef struct _signaltemplate
{
    /* The Object itself */
    t_object obj;
    
    /* Sample-Rate: Many Audio-Objects need to know the current sample-rate, therefore it´s useful to store the sample-rate-value in the data struct. You can initialise this variable in the "signaltemplate_new"-function by calling sys_getsr(). if the sample-rate changes during runtime, pd sends out a "dsp"-message to all objects. it is very important to be able to handle sample-rate changes, so the dsp-function (signaltemplate_dsp) is a good place for that.  */
    t_float sr;
    
    /* Dummy-Float, needed for the MAINSIGNALIN */
    t_float x_f;
    
    /* Example Parameter. The standart type for floating-point variables in puredata is the t_float, defined in m_pd.h. it is handled exactly like a normal float variable. The purpose of using a specific float-variable is to keep consistency while compiling on different platforms. for using integer-variables, you can use "t_int" */
    t_float parameter;
    t_float parameter2;
    
}t_signaltemplate;

//=====================================================================================

void *signaltemplate_new (t_symbol *s, short argc, t_atom *argv);
void signaltemplate_dsp(t_signaltemplate *x, t_signal **sp, short *count);
t_int *signaltemplate_perform(t_int *w);

void signaltemplate_parameter_change(t_signaltemplate *x, t_floatarg f);
void signaltemplate_bang(t_signaltemplate *x);

//=====================================================================================

/* Setup-Routine, called once when the external is loaded into a patch */
void signaltemplate_tilde_setup(void)
{
    signaltemplate_class = class_new(gensym("signaltemplate~"/*must be identical to object-name*/), 
                            (t_newmethod)signaltemplate_new,
                            0,
                            sizeof(t_signaltemplate),
                            CLASS_DEFAULT,
                            A_GIMME,
                            0);
    
    CLASS_MAINSIGNALIN(signaltemplate_class,
                       t_signaltemplate,
                       x_f);
    
    class_addmethod(signaltemplate_class,
                    (t_method)signaltemplate_dsp,
                    gensym("dsp"),
                    0);
    
    class_addmethod(signaltemplate_class,
                    (t_method)signaltemplate_parameter_change,
                    gensym("parameter_change"),
                    A_DEFFLOAT,
                    0);
    
    class_addbang(signaltemplate_class,signaltemplate_bang);
    
    post("signaltemplate~ by David Bau, HS Duesseldorf");
    
}

//=====================================================================================

void *signaltemplate_new (t_symbol *s, short argc, t_atom *argv)
{
    t_signaltemplate *x = (t_signaltemplate*)pd_new(signaltemplate_class);
    
    
    //created an "active" inlet. if a float is sent to it, the function "parameter_changed" will be triggered
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("parameter_change"));
    
    //create a "passive" inlet. if a float is sent to it, it will directly change a certain parameter.
        //--> good side: no extra function needed. bad side: no sanity-checking possible
    floatinlet_new(&x->obj, &x->parameter2);
    
    outlet_new(&x->obj, gensym("signal"));
    
    //Well suited place to initialize variables and get the sample-rate
    x->sr = sys_getsr();
    x->parameter = 0;
    x->parameter2 = 0;


    //Getting creation-arguments
    /*
     If the flag "A_GIMME" is set (in the construcor-call in the setup-function), the ..._new-function will get a list of creation-arguments with arbitrary type, consisting of argc (the number of objects) and argv (the objects in an array)
     To be able to handle the case that the user did not specify all the arguments that you told him to, this routine checks how many arguments have been passed and stores the right values into the right parameters. Note that due to this routine, it is possible to leave out the last or the two last arguments on creation, but not possible to leave out eg. the first one and only pass the last one. (Which makes perfectly sense if you handled objects in pureadata before)
     */
    if (argc>=3) {
        post("didn´t expect that much arguments, but... nevermind");
    }
    if (argc>=2) {
        x->parameter2 = atom_getfloatarg(1, argc, argv);
        post("getting parameter2: %f", x->parameter2);
    }
    if (argc>=1) {
        x->parameter = atom_getfloatarg(0, argc, argv);
        post("getting parameter: %f", x->parameter);
    }
    
    
    return x;
}

//=====================================================================================

/* free function that is called when the object is destroyed */
void signaltemplate_free(t_hsd_allpass *x)
{
    //nothing to do here
}

//=====================================================================================

//Example-Funtion to control a parameter with an active inlet
void signaltemplate_parameter_change(t_signaltemplate *x, t_floatarg f){
    
    /*
     this function will be executed when:
        - a message with the selector "change_parameter" and a value is sent to the leftmost inlet, eg. "change_parameter 500"
        - a float message is sent to the inlet, which has been specified to replace the selector "float" with "change_paramter"
            -> "inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("paramter_change"));"
            -> you have to make sure in the "class_addmethod"-function that the function is expecting a float-value, so you have to set the flag "A_DEFFLOAT"
     
     the latter option is a great way if you want to change a paramter on a certain inlet and also want to do some special tasks (e.g. sanity checks).
    
     */
    post("new parameter_value: %f", x->parameter);
    
}

//=====================================================================================

//Example-function for a standart bang-trigger
void signaltemplate_bang(t_signaltemplate *x){
    
    
    x->parameter = 0;
    
    
    /*
     Whenever a bang-message is sent to any inlet of the object, this funtion is triggered.
     In the same way there are standart-methods for incoming float- and symbol-messages:
     
        - float: "singaltemplate_float(t_signaltemplate *x, floatarg f)"
            (and in the setup-function "class_addfloat(signaltemplate_class,signaltemplate_float);")
     
        - symbol: "singaltemplate_symbol(t_signaltemplate *x, symbol *s)"
            (and in the setup-function "class_addsymbol(signaltemplate_class,signaltemplate_symbol);")
     
     */
}

//=====================================================================================

void signaltemplate_dsp(t_signaltemplate *x, t_signal **sp, short *count)
{
    /*
     When DSP is turned "On" in puredata, a message with the selector "dsp" is sent to all objects, so they can be added to the signal chain. This also happens if audio-changes are done, like changing the sample-rate
     */
    
    
    
    if(x->sr != sp[0]->s_sr){
        /*
         Check for sample rate change. if there is any sample-rate-dependent stuff to recalculate (like a delay-line specified in ms), this is the right place
         */
        x->sr = sp[0]->s_sr;
    }
    
    
    /*
     This function "adds" the perform routine to the signal chain. Its parameters are:
        - the perform routine
        - the number of following parameters
        - the object itself
        - the signal vectors, for every input and output
        - the buffer size, taken from the first sigal-buffer
     */
    dsp_add(signaltemplate_perform,
            4,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[0]->s_n);
}

//=====================================================================================

t_int *signaltemplate_perform(t_int *w)
{
    //In and outlets are adressed clockwise:
    //Inlets from left to right, and then the outlets from right to left
    t_signaltemplate *x =       (t_signaltemplate *) (w[1]);
    t_float *in =           (t_float *) (w[2]);
    t_float *out =        (t_float *) (w[3]);
    t_int n =               w[4];
    
    
    t_float parameter = x->parameter;
    t_float parameter2 = x->parameter2;
    
    
    while (n--) {
        
        *out++ =  *in++;
        
    }
    
    
    return w+5;
}
