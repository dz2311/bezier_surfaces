// Very simple display triangle program, that allows you to rotate the
// triangle around the Y axis.
//
// This program does NOT use a vertex shader to define the vertex colors.
// Instead, it computes the colors in the display callback (using Blinn/Phong)
// and passes those color values, one per vertex, to the vertex shader, which
// passes them directly to the fragment shader. This achieves what is called
// "gouraud shading".

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/freeglut_ext.h>
#endif

#include <vector>
#include <limits>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <string>
#include "amath.h"

//const int NumVertices = 3;
int NumVertices;
int sample_times;

typedef amath::vec4  point4;
typedef amath::vec4  color4;

float theta = 0.0;  // rotation around the Y (up) axis
float theta_lat = 0.0;
float posx = 0.0;   // translation along X
float posy = 0.0;   // translation along Y

GLuint buffers[2];
GLint matrix_loc;

std::vector<int> cpoints_ids;
std::vector<float> control_verts;

std::vector<point4> vertices;

point4 * verts;
vec4 * norms;
// viewer's position, for lighting calculations
vec4 viewer = vec4(0.0, 0.0, 10.0, 1.0);

// light & material definitions, again for lighting calculations:
point4 light_position = point4(100.0, 100.0, 100.0, 1.0);
color4 light_ambient = color4(0.2, 0.2, 0.2, 1.0);
color4 light_diffuse = color4(1.0, 1.0, 1.0, 1.0);
color4 light_specular = color4(1.0, 1.0, 1.0, 1.0);

color4 material_ambient = color4(1.0, 0.0, 1.0, 1.0);
color4 material_diffuse = color4(1.0, 0.8, 0.0, 1.0);
color4 material_specular = color4(1.0, 0.8, 0.0, 1.0);
float material_shininess = 100.0;

// we will copy our transformed points to here:
std::vector<point4> points;
std::vector<point4> colors;

static int lastx;// keep track of where the mouse was last:
static int lasty;
static float delta_lat;
// and we will store the colors, per face per vertex, here. since there is
// only 1 triangle, with 3 vertices, there will just be 3 here:
//color4 colors[NumVertices];

// a transformation matrix, for the rotation, which we will apply to every
// vertex:
mat4 ctm;

mat4 YRotate, XRotate, ViewP, ViewL,mat_default;

GLuint program; //shaders


// product of components, which we will use for shading calculations:
vec4 product(vec4 a, vec4 b)
{
    return vec4(a[0]*b[0], a[1]*b[1], a[2]*b[2], a[3]*b[3]);
}

//read in the self-defined text file
int read_scene_file(const char *file, std::vector< int > &cpoints_ids, std::vector< float > &control_verts)
{
    cpoints_ids.clear ();
    control_verts.clear ();
    int markLine = 2;
    std::ifstream in(file);
    int count_curves = 0;
    in >> count_curves;
    while(!in.eof())
    {
        if(!in.eof())
        {
            int trs = 0;
            in >> trs;
            if(trs>0)
            {
                markLine = trs+1;
                cpoints_ids.push_back(trs);
                in >> trs;
                cpoints_ids.push_back(trs);
                int count = 0;
        
                while(!in.eof()&&count<markLine*(trs+1)*3)
                {
                    float trs2 = 0.0;
                    in >> trs2;
                    control_verts.push_back(trs2);
                    count++;
                }
            }
        }
    }
    in.close();
    std::cout << "found this many control points: " << control_verts.size () / 3.0 << std::endl;
    return count_curves;
}
point4 findPoint(point4 point_1, point4 point_2, float t)
{
    point4 trs = point_1 * (1- t) + point_2 * t;
    point4 res = vec4(trs[0], trs[1], trs[2], 1.0);
    return res;
}
//interpolatint points
void eval_bez (const std::vector<point4> controlpoints, int degree, const double t, point4 &pnt, vec4 &tangent)
{
    std::vector<point4> buffer_arr;
    if(controlpoints.size()==2)
    {
        vec4 trs = controlpoints[1]-controlpoints[0];
        tangent = vec4(trs[0], trs[1], trs[2], 0.0);
    }
    for(int k=1; k<controlpoints.size();++k)
    {
        point4 next_point = findPoint(controlpoints[k-1], controlpoints[k], t);
        buffer_arr.push_back(next_point);
    }
    while(buffer_arr.size()>1)
    {
        if(buffer_arr.size()==2)
        {
            vec4 trs = buffer_arr[1]-buffer_arr[0];
            tangent = vec4(trs[0], trs[1], trs[2], 0.0);
        }
        std::vector<point4> buffer_arr2;
        for(int k=1; k<buffer_arr.size();++k)
        {
            point4 next_point = findPoint(buffer_arr[k-1], buffer_arr[k], t);
            buffer_arr2.push_back(next_point);
        }
        buffer_arr.clear();
        for(int k=0;k<buffer_arr2.size();++k)
            buffer_arr.push_back(buffer_arr2[k]);
    }
    pnt = buffer_arr[0];
}
// initialization: set up a Vertex Array Object (VAO) and then
void init()
{
    
    // create a vertex array object - this defines mameory that is stored
    // directly on the GPU
    GLuint vao;
    
    // deending on which version of the mac OS you have, you may have to do this:
#ifdef __APPLE__
    glGenVertexArraysAPPLE( 1, &vao );  // give us 1 VAO:
    glBindVertexArrayAPPLE( vao );      // make it active
#else
    glGenVertexArrays( 1, &vao );   // give us 1 VAO:
    glBindVertexArray( vao );       // make it active
#endif
    
      glGenBuffers(1, buffers);
      glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);  // make it active
      glBufferData(GL_ARRAY_BUFFER, sizeof(vec4)*NumVertices*2, NULL, GL_STATIC_DRAW);
    
    program = InitShader("vshader_passthrough.glsl", "fshader_passthrough.glsl");
 
    // ...and set them to be active
    glUseProgram(program);
    GLuint loc, loc2;
    
    loc = glGetAttribLocation(program, "vPosition");
    glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(loc);
    
    loc2 = glGetAttribLocation(program, "vColor");
    glVertexAttribPointer(loc2, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sizeof(vec4)*NumVertices));
    glEnableVertexAttribArray(loc2);
    
    glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof(point4)*NumVertices, &verts[0]);
    glBufferSubData( GL_ARRAY_BUFFER, sizeof(point4)*NumVertices,sizeof(vec4)*NumVertices, &norms[0]);
    
    glUniform4fv(glGetUniformLocation(program, "Viewer"), 1, (GLfloat*)viewer);
    
    glUniform4fv(glGetUniformLocation(program, "LightPosition"), 1, (GLfloat*)light_position);
    glUniform4fv(glGetUniformLocation(program, "LightAmbient"), 1, (GLfloat*)light_ambient);
    glUniform4fv(glGetUniformLocation(program, "LightDiffuse"), 1, (GLfloat*)light_diffuse);
    glUniform4fv(glGetUniformLocation(program, "LightSpecular"), 1, (GLfloat*)light_specular);

    glUniform4fv(glGetUniformLocation(program, "MaterialAmbient"), 1, (GLfloat*)material_ambient);
    glUniform4fv(glGetUniformLocation(program, "MaterialDiffuse"), 1, (GLfloat*)material_diffuse);
    glUniform4fv(glGetUniformLocation(program, "MaterialSpecular"), 1, (GLfloat*) material_specular);
    glUniform1f(glGetUniformLocation(program, "MaterialShininess"), material_shininess);
    

    // set the background color (white)
    glClearColor(1.0, 1.0, 1.0, 1.0);
}

/*
 *******The funtion is used to calculate the sampling point and the normal of the point ********
 ***********************************************************************************************
 */
void curveIntersection(const std::vector<point4> surface, int u_degree, int v_degree, float u, float v, point4 &pnt, vec4 &nor)
{
    std::vector<point4> arr_points;
    arr_points.clear();
   
    for(int k=0; k<v_degree+1; ++k) // scan each row first, for each row
    {
        std::vector<point4> row_u;
        row_u.clear();
        
        for(int j=0; j<u_degree+1;++j) //push each element in a row into the array row_u;
        {
            row_u.push_back(surface[k*(u_degree+1)+j]);
        }
        point4 pnt_row_u = point4(0.0,0.0,0.0,1.0);
        vec4 tangent_row_u = vec4(0.0,0.0,0.0,0.0);
        
        eval_bez (row_u, u_degree, u, pnt_row_u, tangent_row_u);
        
        arr_points.push_back(pnt_row_u);
    }
    point4 pnt_u = point4(0.0,0.0,0.0,1.0);
    vec4 tangent_u = vec4(0.0,0.0,0.0,0.0); // this actually get tagent v
    
    eval_bez (arr_points, v_degree, v, pnt_u, tangent_u);
    pnt = pnt_u;
    //tangent_u = normalize(tangent_u);
    
    //clear the array
    arr_points.clear();
    //then scan each column first, for each column
    for(int k=0; k<u_degree+1; ++k) // scan each row first, for each row
    {
        std::vector<point4> row_v;
        row_v.clear();
        
        for(int j=0; j<v_degree+1;++j) //push each element in a row into the array row_u;
        {
            row_v.push_back(surface[j*(u_degree+1)+k]);
        }
        point4 pnt_row_v = point4(0.0,0.0,0.0,1.0);
        vec4 tangent_row_v = vec4(0.0,0.0,0.0,0.0);
        
        eval_bez (row_v, v_degree, v, pnt_row_v, tangent_row_v);
        
        arr_points.push_back(pnt_row_v);
    }
    point4 pnt_v = point4(0.0,0.0,0.0,1.0);
    vec4 tangent_v = vec4(0.0,0.0,0.0,0.0);  //this actually get tangent u
    
    eval_bez (arr_points, u_degree, u, pnt_v, tangent_v);
    
    //tangent_v = normalize(tangent_v);
    nor = vec4(normalize(cross(tangent_u, tangent_v)), 0.0);
    
}
/*
 *******The funtion is used to put the vector of points and normals into ********
 *******the whole vector into VBO    ********************************************
*/
void putIntoTri(std::vector<point4> arr_points, std::vector<vec4> arr_normals, int num_row, int num_column)
{
    // put points into points[], put normals into colors[]
    for(int j=0; j<num_column-1; ++j)
    {
        for(int i=0; i<num_row; ++i)
        {
            if(i==0)
            {
                points.push_back(arr_points[j*num_row]);
                colors.push_back(arr_normals[j*num_row]);
                points.push_back(arr_points[j*num_row+1]);
                colors.push_back(arr_normals[j*num_row+1]);
                points.push_back(arr_points[(j+1)*num_row]);
                colors.push_back(arr_normals[(j+1)*num_row]);
            }
            else if(i==num_row-1)
            {
                points.push_back(arr_points[j*num_row+i]);
                colors.push_back(arr_normals[j*num_row+i]);
                points.push_back(arr_points[(j+1)*num_row+i]);
                colors.push_back(arr_normals[(j+1)*num_row+i]);
                points.push_back(arr_points[(j+1)*num_row+i-1]);
                colors.push_back(arr_normals[(j+1)*num_row+i-1]);
            }
            else
            {
                points.push_back(arr_points[j*num_row+i]);
                colors.push_back(arr_normals[j*num_row+i]);
                points.push_back(arr_points[j*num_row+i+1]);
                colors.push_back(arr_normals[j*num_row+i+1]);
                points.push_back(arr_points[(j+1)*num_row+i]);
                colors.push_back(arr_normals[(j+1)*num_row+i]);
                
                points.push_back(arr_points[j*num_row+i]);
                colors.push_back(arr_normals[j*num_row+i]);
                points.push_back(arr_points[(j+1)*num_row+i]);
                colors.push_back(arr_normals[(j+1)*num_row+i]);
                points.push_back(arr_points[(j+1)*num_row+i-1]);
                colors.push_back(arr_normals[(j+1)*num_row+i-1]);
            }
        }
    }

}
/*
*******The funtion is used to deal with sampling for each pair of (u, v)********
*******Only for one surface per time********************************************
 */
void tri(int u_degree, int v_degree,const std::vector<point4> surface)
{
    float delta_u = 1/(float)((u_degree+1)*sample_times-1);
    float delta_v = 1/(float)((v_degree+1)*sample_times-1);
    
    std::vector<point4> points_curve;
    std::vector<vec4> normals_curve;
    
    points_curve.clear();
    normals_curve.clear();
    
    float u=0.0, v=0.0;
    //sampling
    for(v=0.0;v<=1.0;v+=delta_v)
    {
        for(u=0.0;u<=1.0;u+=delta_u)
        {
            //for a specific pair of (u, v)
            point4 pnt = point4(0.0,0.0,0.0,1.0);
            vec4 nor = vec4(0.0,0.0,0.0,0.0);
            curveIntersection(surface, u_degree, v_degree, u, v, pnt, nor);
            points_curve.push_back(pnt);
            normals_curve.push_back(nor);
        }
    }
    // There are (u_degree+1)*sample * (v_degree+1) * sample points in total
    putIntoTri(points_curve, normals_curve, (u_degree+1)*sample_times, (v_degree+1)*sample_times);
}
void reSample()
{
    points.clear();
    colors.clear();
    
    int arr_offset = 0;
    delete [] verts;
    delete [] norms;
    //for multiple surfaces, get the whole vector control_verts and control_ids
    //seperate them into corresponding part of surfaces, then use tri() to get
    for(int i=0;i<cpoints_ids.size();i+=2)
    {
        vertices.clear(); // for storing points of each surface
        int next_length = (cpoints_ids[i]+1)*(cpoints_ids[i+1]+1);
        for(int k=0;k<next_length;++k)
        {
            int row = cpoints_ids[i+1] - k/(cpoints_ids[i]+1);
            int column = k%(cpoints_ids[i]+1);
            point4 trs = point4(control_verts[arr_offset*3 + 3*(row*(cpoints_ids[i]+1)+column)], control_verts[arr_offset*3 + 3*(row*(cpoints_ids[i]+1)+column)+1], control_verts[arr_offset*3 + 3*(row*(cpoints_ids[i]+1)+column)+2], 1.0);
            vertices.push_back(trs);
        }
        tri(cpoints_ids[i], cpoints_ids[i+1], vertices);
        arr_offset += next_length;
    }
    NumVertices = (int)points.size();
    verts = new point4[NumVertices];
    norms = new vec4[NumVertices];
    for (int in = 0; in < NumVertices; ++in)
    {
        norms[in] = vec4(0.0, 0.0, 0.0, 0.0);
    }
    
    for (unsigned int in = 0; in < NumVertices; ++in)
    {
        verts[in] = vec4(0.0, 0.0, 0.0, 0.0);
    }
    
    for(int i=0;i<NumVertices;++i)
    {
        verts[i] = point4(points[i][0], points[i][1], points[i][2], 1.0);
        norms[i] = vec4(colors[i][0], colors[i][1], colors[i][2], 0.0);
    }
    glutPostRedisplay();

}

void display( void )
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //reSample();
    
//    YRotate = amath::RotateY(theta);
//    XRotate = amath::RotateX(theta_lat);
//    ViewP = Perspective(40.0, 1.0, 1.0, 50.0);
//    
//    viewer =  XRotate * YRotate * viewer;
    //
    viewer = RotateY(theta) * viewer;
    // Axis X
    vec4 neweye = RotateX(theta_lat) * viewer;
    float angleZenit = acos(dot(normalize(neweye - vec4(0.0,0.0,0.0,1.0)), vec4(0,1,0,0))) / DegreesToRadians;
    float angleNadir = acos(dot(normalize(neweye - vec4(0.0,0.0,0.0,1.0)), vec4(0,-1,0,0))) / DegreesToRadians;
    if (angleZenit >= 5 && angleNadir >= 5 && angleZenit <= 355 && angleNadir <= 355)
        viewer = neweye;
    ViewL = LookAt(viewer, vec4(0.0,0.0,0.0,1.0), vec4(0.0,1.0,0.0,0.0));
    ViewP = Perspective(40.0, 1.0, 1.0, 50.0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts[0])*NumVertices+sizeof(norms[0])*NumVertices, NULL, GL_STATIC_DRAW);
    //std::cout<<"The size "<<sizeof(verts[0])*NumVertices*2<<std::endl;
    glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof(verts[0])*NumVertices, &verts[0]);
    glBufferSubData( GL_ARRAY_BUFFER, sizeof(verts[0])*NumVertices,sizeof(norms[0])*NumVertices, &norms[0]);
    
    glUseProgram(program);
    
    GLuint loc, loc2;
    
    loc = glGetAttribLocation(program, "vPosition");
    glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    glEnableVertexAttribArray(loc);
    
    loc2 = glGetAttribLocation(program, "vColor");
    glVertexAttribPointer(loc2, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sizeof(point4)*NumVertices));
    glEnableVertexAttribArray(loc2);
    
    glUniform4fv(glGetUniformLocation(program, "Viewer"), 1, (GLfloat*)viewer);
    glUniformMatrix4fv(glGetUniformLocation(program, "ViewPer"), 1, GL_TRUE, ViewP);
    glUniformMatrix4fv(glGetUniformLocation(program, "ViewLook"), 1, GL_TRUE, ViewL);
//    glUniformMatrix4fv(glGetUniformLocation(program, "RotateYAixs"), 1, GL_TRUE, YRotate);
//    glUniformMatrix4fv(glGetUniformLocation(program, "RotateXAxis"), 1, GL_TRUE, XRotate);
    
    glDrawArrays(GL_TRIANGLES, 0, NumVertices);
    glutSwapBuffers();
   
}


// use this motionfunc to demonstrate rotation - it adjusts "theta" based
// on how the mouse has moved. Theta is then used the the display callback
// to generate the transformation, ctm, that is applied
// to all the vertices before they are displayed:
void mouse_move_rotate (int x, int y)
{
    
    int amntX = (x - lastx);
    if (amntX != 0) {
        theta =  amntX;
        if (theta > 360.0 ) theta -= 360.0;
        if (theta < 0.0 ) theta += 360.0;
        
        lastx = x;
    }
    //theta_lat = 0;
    int amntY = (y - lasty);
    if(amntY != 0) {
//        delta_lat += (amntY);
//        if(delta_lat < 85.0&&delta_lat>-85.0)
            theta_lat = (amntY);
        if (theta_lat > 360.0 ) theta_lat -= 360.0;
        if (theta_lat < 0.0 ) theta_lat += 360.0;
        
        lasty = y;
   }

    // force the display routine to be called as soon as possible:
    glutPostRedisplay();
    
}

void MyMouse(int button, int state, int x, int y)
{
    
    switch (button)
    {
        case GLUT_LEFT_BUTTON:
            
            if(state == GLUT_DOWN)
            {
                lastx = x;
                lasty = y;
                theta_lat = 0.0;
                theta = 0.0;
            }
            break;
    }
    
}

// the keyboard callback, called whenever the user types something with the
// regular keys.
void mykey(unsigned char key, int mousex, int mousey)
{
    if(key=='q'||key=='Q')
    {
        delete [] verts;
        delete [] norms;
        exit(0);
    }
    
    // and r resets the view:
    if (key =='r') {
        posx = 0;
        posy = 0;
        theta = 0;
        glutPostRedisplay();
    }
    
    if(key =='z') {
        if(viewer[2]>2)
        {
            viewer = vec4(viewer[0], viewer[1], viewer[2]-1, 1.0);
            glutPostRedisplay();
        }
    }
    if(key=='x'){
        if (viewer[2]<50) {
            viewer = vec4(viewer[0], viewer[1], viewer[2]+1, 1.0);
             glutPostRedisplay();
        }
    }
    if(key=='<'||key=='>')
    {
        if(key=='<'&&sample_times<10)
        {
            sample_times++;
            if(sample_times==3)
                sample_times++;
            reSample();
        }
        else if(key=='>'&&sample_times>1)
        {
            sample_times--;
            if(sample_times==3)
                sample_times--;
            reSample();
        }
        else
        {
            return;
        }
    }
}



int main(int argc, char** argv)
{
    //read the given file to get the vertexes info
    lastx = 0;
    lasty = 0;
    int num_curves = 0;
    sample_times = 2;
    num_curves = read_scene_file(argv[1], cpoints_ids, control_verts);
//    std::cout<<"Number of vertices"<<NumVertices<<std::endl;
//      std::cout<<"Number of curves"<<num_curves<<std::endl;
//    std::cout << "Number of cpoints" <<cpoints_ids.size()<<std::endl;
//    for(int k=0;k<cpoints_ids.size();++k)
//        printf("%d  ", cpoints_ids[k]);
    points.clear();
    colors.clear();
    int arr_offset = 0;
    //for multiple surfaces, get the whole vector control_verts and control_ids
    //seperate them into corresponding part of surfaces, then use tri() to get
    for(int i=0;i<cpoints_ids.size();i+=2)
    {
        vertices.clear(); // for storing points of each surface
        int next_length = (cpoints_ids[i]+1)*(cpoints_ids[i+1]+1);
        for(int k=0;k<next_length;++k)
        {
            int row = cpoints_ids[i+1] - k/(cpoints_ids[i]+1);
            int column = k%(cpoints_ids[i]+1);
            point4 trs = point4(control_verts[arr_offset*3 + 3*(row*(cpoints_ids[i]+1)+column)], control_verts[arr_offset*3 + 3*(row*(cpoints_ids[i]+1)+column)+1], control_verts[arr_offset*3 + 3*(row*(cpoints_ids[i]+1)+column)+2], 1.0);
            vertices.push_back(trs);
        }
        tri(cpoints_ids[i], cpoints_ids[i+1], vertices);
        arr_offset += next_length;
    }
    NumVertices = (int)points.size();
    verts = new point4[NumVertices];
    norms = new vec4[NumVertices];
    for (int in = 0; in < NumVertices; ++in)
    {
        norms[in] = vec4(0.0, 0.0, 0.0, 0.0);
    }
    
    for (unsigned int in = 0; in < NumVertices; ++in)
    {
        verts[in] = vec4(0.0, 0.0, 0.0, 1.0);
    }
    
    for(int i=0;i<NumVertices;++i)
    {
        verts[i] = point4(points[i][0], points[i][1], points[i][2], 1.0);
        norms[i] = vec4(colors[i][0], colors[i][1], colors[i][2], 0.0);
    }
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH | GLUT_DOUBLE);
    
    // give us a window in which to display, and set its title:
    glutInitWindowSize(512, 512);
    glutCreateWindow("Rotate / Translate Triangle");
 
    // for displaying things, here is the callback specification:
    glutDisplayFunc(display);
    
    glutMotionFunc(mouse_move_rotate);
    glutMouseFunc(MyMouse);
    // for any keyboard activity, here is the callback:
    glutKeyboardFunc(mykey);
    
#ifndef __APPLE__
    // initialize the extension manager: sometimes needed, sometimes not!
    glewInit();
#endif

    // call the init() function, defined above:
    init();
    
    // enable the z-buffer for hidden surface removel:
    glEnable(GL_DEPTH_TEST);

    // once we call this, we no longer have control except through the callbacks:
    glutMainLoop();
    return 0;
}
