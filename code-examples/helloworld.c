/* helloworld.c
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

 This is a super simple external to give an overview of the basic structure of an Pd external. 
 It
 
 */

#include "m_pd.h"

t_class *example_class;

    //DATA
///////////////////////////////////////////////////////////////////////
typedef struct _example                                              //
{                                                                    //
    t_object obj;                                                    //
    t_float value;                                                   //
}t_example;                                                          //
///////////////////////////////////////////////////////////////////////

    //FUNCTIONS
//////////////////////////////////////////////////////////////////////////////////
                                                                                //
/* called whenever the object receives a float-message */                       //
void example_float(t_example *x, t_floatarg f)                                  //
{                                                                               //
    x->value = f;                                                               //
}                                                                               //
                                                                                //
/* called whenever the object receives a message with a "hello"-specifier */    //
void example_hello(t_example *x)                                                //
{                                                                               //
    post("HelloWorld");                                                         //
}                                                                               //
                                                                                //
//////////////////////////////////////////////////////////////////////////////////

    //INITIALISATIONS
//////////////////////////////////////////////////////////////////////////////////////
                                                                                    //
/* called ONCE, when the external is loaded the first time by PD */                 //
void example_setup(void)                                                            //
{                                                                                   //
    example_class = class_new(gensym("example"),                                    //
                              (t_newmethod)example_new,                             //
                              0,                                                    //
                              sizeof(t_example),                                    //
                              0,                                                    //
                              0);                                                   //
                                                                                    //
    class_addmethod(example_class, (t_method)example_hello, gensym("hello"), 0);    //
    class_addfloat(example_class, example_float);                                   //
}                                                                                   //
                                                                                    //
/* called for every newly created object */                                         //
void *example_new(void)                                                             //
{                                                                                   //
    t_example *x = (t_example *)pd_new(example_class);                              //
    x->value = 0;                                                                   //
    return (void *)x;                                                               //
}                                                                                   //
                                                                                    //
//////////////////////////////////////////////////////////////////////////////////////