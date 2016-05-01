// a contrived vertex shader, that colors each vertex in a triangle
// according to its position

// we are going to be getting an attribute from the main program, named
// "vPosition", one for each vertex.
//
// on mac you may have to say: "attribute vec4 vPosition;" instead of this:
attribute vec4 vPosition;
attribute vec4 vColor;
// we are also going to be getting color values, per-vertex, from the main
// program running on the cpu (and that are then stored in the VBO on the
// card. this color, called "vColor", is just going to be passed through 
// to the fragment shader, which will intrpolate its value per-fragment
// amongst the 3 vertex colors on the triangle that fragment is a part of.
//
// on mac you may have to say: "attribute vec4 vColor;" instead of this:
//attribute vec4 vColor;

// we are going to be outputting a single 4-vector, called color, which
// may be different for each vertex.
// the fragment shader will be expecting these values, and interpolate
// them according to the distance of the fragment from the vertices
//
// on mac you may have to say: "varying vec4 color;" instead of this:

uniform vec4 Viewer;
//
uniform vec4 LightPosition;
uniform vec4 LightAmbient;
uniform vec4 LightDiffuse;
uniform vec4 LightSpecular;
//
uniform vec4 MaterialAmbient;
uniform vec4 MaterialDiffuse;
uniform vec4 MaterialSpecular;
uniform float MaterialShininess;
//
//uniform mat4 RotateYAixs;
//uniform mat4 RotateXAxis;
uniform mat4 ViewLook;
uniform mat4 ViewPer;

varying vec4 color;
void main() 
{
    mat4 trs = ViewPer*ViewLook;
    //RotateYAixs*RotateXAxis;
    
    gl_Position = trs * vPosition;
    vec4 nor = vColor;
    
    vec3 n =  normalize(nor.xyz);
    vec3 l = normalize(LightPosition.xyz-vPosition.xyz);
    vec3 v = normalize(Viewer.xyz-vPosition.xyz);
    vec3 h = normalize(v+l);
    vec4 ambient_color, diffuse_color, specular_color;
    
    ambient_color = MaterialAmbient*LightAmbient;
    float dd = dot(l, n);
    
    if(dd>0.0) diffuse_color = dd * LightDiffuse * MaterialDiffuse;
    else diffuse_color =  vec4(0.0, 0.0, 0.0, 1.0);
    
    dd = dot(h, n);
    if(dd > 0.0) specular_color = pow(dd, MaterialShininess)* LightSpecular * MaterialSpecular;
    else specular_color = vec4(0.0, 0.0, 0.0, 1.0);
    color = specular_color+diffuse_color+ambient_color;
    //color = vec4(0.5,0.5,0.5,1.0);
}
