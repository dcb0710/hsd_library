/* signaltemplate~uncommented.c
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

 */

#include "m_pd.h"


static t_class *signaltemplate_class;

typedef struct _signaltemplate
{
    t_object obj;
    t_float sr;
    t_float x_f;
    t_float parameter;
    t_int parameter2;
    
}t_signaltemplate;

void *signaltemplate_new (t_symbol *s, short argc, t_atom *argv);
void signaltemplate_dsp(t_signaltemplate *x, t_signal **sp, short *count);
t_int *signaltemplate_perform(t_int *w);

void signaltemplate_parameter_change(t_signaltemplate *x, t_floatarg f);
void signaltemplate_bang(t_signaltemplate *x);



void signaltemplate_tilde_setup(void)
{
    signaltemplate_class = class_new(gensym("signaltemplate~"),
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
                    (t_method)signaltemplate_change_parameter,
                    gensym("parameter_change"),
                    A_DEFFLOAT,
                    0);
    
    class_addbang(signaltemplate_class,signaltemplate_bang);
    
    post("signaltemplate~ by David Bau, HS Duesseldorf");
    
}

void *signaltemplate_new (t_symbol *s, short argc, t_atom *argv)
{
    t_signaltemplate *x = (t_signaltemplate*)pd_new(signaltemplate_class);
    
    
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("float"), gensym("parameter_change"));
    
    
    outlet_new(&x->obj, gensym("signal"));
    
    x->sr = sys_getsr();
    x->parameter = 0;
    x->parameter2 = 0;


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

void signaltemplate_parameter_change(t_signaltemplate *x, t_floatarg f){
    
    post("new parameter_value: %f", x->parameter);
    
}


void signaltemplate_bang(t_signaltemplate *x){
    
    
}

void signaltemplate_dsp(t_signaltemplate *x, t_signal **sp, short *count)
{
    
    
    if(x->sr != sp[0]->s_sr){
       x->sr = sp[0]->s_sr;
    }
    
    
    dsp_add(signaltemplate_perform,
            4,
            x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[0]->s_n);
}


t_int *signaltemplate_perform(t_int *w)
{
    
    t_signaltemplate *x =       (t_signaltemplate *) (w[1]);
    t_float *in =           (t_float *) (w[2]);
    t_float *out =        (t_float *) (w[3]);
    t_int n =               w[4];
    
    
    t_float parameter = x->parameter;
    
    while (n--) {
        
        *out++ =  *in++;
        
    }
    
    
    return w+5;
}
