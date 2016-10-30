#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/primvarRefiner.h>

// Include standard headers
#include <iostream>
#include <stdlib.h>
#include <stdio.h>

// Include GLFW
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>

GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include "shader.hpp"


//------------------------------------------------------------------------------
// Vertex container implementation.
//
struct Vertex {
    
    // Minimal required interface ----------------------
    Vertex() { }
    
    Vertex(Vertex const & src) {
        _position[0] = src._position[0];
        _position[1] = src._position[1];
        _position[2] = src._position[2];
    }
    
    void Clear( void * =0 ) {
        _position[0]=_position[1]=_position[2]=0.0f;
    }
    
    void AddWithWeight(Vertex const & src, float weight) {
        _position[0]+=weight*src._position[0];
        _position[1]+=weight*src._position[1];
        _position[2]+=weight*src._position[2];
    }
    
    // Public interface ------------------------------------
    void SetPosition(float x, float y, float z) {
        _position[0]=x;
        _position[1]=y;
        _position[2]=z;
    }
    
    const float * GetPosition() const {
        return _position;
    }
    
private:
    float _position[3];
};

//------------------------------------------------------------------------------
// Cube geometry from catmark_cube.h
static float g_verts[8][3] = {{ -0.5f, -0.5f,  0.5f },
    {  0.5f, -0.5f,  0.5f },
    { -0.5f,  0.5f,  0.5f },
    {  0.5f,  0.5f,  0.5f },
    { -0.5f,  0.5f, -0.5f },
    {  0.5f,  0.5f, -0.5f },
    { -0.5f, -0.5f, -0.5f },
    {  0.5f, -0.5f, -0.5f }};

static int g_nverts = 8,
g_nfaces = 6;

static int g_vertsperface[6] = { 4, 4, 4, 4, 4, 4 };

static int g_vertIndices[24] = { 0, 1, 3, 2,
    2, 3, 5, 4,
    4, 5, 7, 6,
    6, 7, 1, 0,
    1, 7, 5, 3,
    6, 0, 2, 4  };

float rot = 0.0f;

using namespace OpenSubdiv;

int main( void )
{
    // Initialise GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        getchar();
        return -1;
    }
    
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Open a window and create its OpenGL context
    window = glfwCreateWindow( 1024, 768, "Tutorial 04 - Colored Cube", NULL, NULL);
    if( window == NULL ){
        fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
        getchar();
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    
    // Dark blue background
    glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
    
    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);
    
    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);
    
    // Create and compile our GLSL program from the shaders
    GLuint programID = LoadShaders( "/Users/hasegawayasuo/Desktop/misc/test_code/c++/161028/opensaubdiv_test2/opensaubdiv_test2/TransformVertexShader.vertexshader", "/Users/hasegawayasuo/Desktop/misc/test_code/c++/161028/opensaubdiv_test2/opensaubdiv_test2/ColorFragmentShader.fragmentshader" );
    
    // Get a handle for our "MVP" uniform
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");
    
    // ==========- opensubdiv ==========
    typedef Far::TopologyDescriptor Descriptor;
    
    Sdc::SchemeType type = OpenSubdiv::Sdc::SCHEME_CATMARK;
    
    Sdc::Options options;
    options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);
    
    Descriptor desc;
    desc.numVertices  = g_nverts;
    desc.numFaces     = g_nfaces;
    desc.numVertsPerFace = g_vertsperface;
    desc.vertIndicesPerFace  = g_vertIndices;
    
    
    // Instantiate a FarTopologyRefiner from the descriptor
    Far::TopologyRefiner * refiner = Far::TopologyRefinerFactory<Descriptor>::Create(desc,
                                                                                     Far::TopologyRefinerFactory<Descriptor>::Options(type, options));
    
    int maxlevel = 2;
    
    // Uniformly refine the topolgy up to 'maxlevel'
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxlevel));
    
    // Allocate a buffer for vertex primvar data. The buffer length is set to
    // be the sum of all children vertices up to the highest level of refinement.
    std::vector<Vertex> vbuffer(refiner->GetNumVerticesTotal());
    Vertex * verts = &vbuffer[0];
    
    
    // Initialize coarse mesh positions
    int nCoarseVerts = g_nverts;
    for (int i=0; i<nCoarseVerts; ++i) {
        verts[i].SetPosition(g_verts[i][0], g_verts[i][1], g_verts[i][2]);
    }
    
    
    // Interpolate vertex primvar data
    Far::PrimvarRefiner primvarRefiner(*refiner);
    
    Vertex * src = verts;
    for (int level = 1; level <= maxlevel; ++level) {
        Vertex * dst = src + refiner->GetLevel(level-1).GetNumVertices();
        primvarRefiner.Interpolate(level, src, dst);
        src = dst;
    }

    std::vector<glm::vec3> vertexBufferData;
    std::vector<glm::vec3> colorBufferData;
    std::vector<unsigned int> vertexIndices;
    std::vector<glm::vec3> temp_vertices;
    int maxLen = 0;
    { // Output OBJ of the highest level refined -----------
        
        Far::TopologyLevel const & refLastLevel = refiner->GetLevel(maxlevel);
        
        int nverts = refLastLevel.GetNumVertices();
        int nfaces = refLastLevel.GetNumFaces();
        maxLen = nverts;
        // Print vertex positions
        int firstOfLastVerts = refiner->GetNumVerticesTotal() - nverts;
        
        for (int vert = 0; vert < nverts; ++vert) {
            float const * pos = verts[firstOfLastVerts + vert].GetPosition();
            
            temp_vertices.push_back(glm::vec3(pos[0]*3.0f,pos[1]*3.0f,pos[2]*3.0f));
            
            printf("v %f %f %f %i\n", pos[0], pos[1], pos[2], firstOfLastVerts + vert);
        }
        
        // Print faces
        for (int face = 0; face < nfaces; ++face) {
            
            Far::ConstIndexArray fverts = refLastLevel.GetFaceVertices(face);
            
            // all refined Catmark faces should be quads
            assert(fverts.size()==4);
            
            printf("f ");
            for (int vert=0; vert<fverts.size(); ++vert) {
                vertexIndices.push_back(fverts[vert]+1);
                printf("%d ", fverts[vert]+1); // OBJ uses 1-based arrays...
            }
            printf("\n");
        }
        
        for( unsigned int i=0; i<vertexIndices.size(); i++ ){
            unsigned int vertexIndex = vertexIndices[i];
            glm::vec3 vertex = temp_vertices[ vertexIndex-1 ];
            vertexBufferData.push_back(vertex);
            colorBufferData.push_back(glm::vec3(0.583f+(float)i*0.001,0.371f+(float)i*0.001,0.214f+(float)i*0.001));
        }
    }
    // ==========- / opensubdiv ==========
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, vertexBufferData.size() * sizeof(glm::vec3), &vertexBufferData[0], GL_STATIC_DRAW);
    
    GLuint colorbuffer;
    glGenBuffers(1, &colorbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
    glBufferData(GL_ARRAY_BUFFER, colorBufferData.size() * sizeof(glm::vec3), &colorBufferData[0], GL_STATIC_DRAW);
    
    double lastTime = glfwGetTime();
    double lastFrameTime = lastTime;
    int nbFrames = 0;
    
    do{
        
        // Measure speed
        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - lastFrameTime);
        lastFrameTime = currentTime;
        nbFrames++;
        if ( currentTime - lastTime >= 1.0 ){ // If last prinf() was more than 1sec ago
            // printf and reset
            printf("%f ms/frame\n", 1000.0/double(nbFrames));
            nbFrames = 0;
            lastTime += 1.0;
        }
        
        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Use our shader
        glUseProgram(programID);
        
        
        // Projection matrix : 45° Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
        glm::mat4 Projection = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
        // Camera matrix
        glm::mat4 View       = glm::lookAt(
                                           glm::vec3(4,3,-3), // Camera is at (4,3,-3), in World Space
                                           glm::vec3(0,0,0), // and looks at the origin
                                           glm::vec3(0,1,0)  // Head is up (set to 0,-1,0 to look upside-down)
                                           );
        rot += 3.14159f/2.0f * deltaTime;
        // Model matrix : an identity matrix (model will be at the origin)
        glm::mat4 Model      = glm::mat4(1.0f);
        Model = glm::rotate(Model, rot, glm::vec3(1.0));
        // Our ModelViewProjection : multiplication of our 3 matrices
        glm::mat4 MVP        = Projection * View * Model; // Remember, matrix multiplication is the other way around
        
        
        // Send our transformation to the currently bound shader,
        // in the "MVP" uniform
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
        
        // 1rst attribute buffer : vertices
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glVertexAttribPointer(
                              0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
                              3,                  // size
                              GL_FLOAT,           // type
                              GL_FALSE,           // normalized?
                              0,                  // stride
                              (void*)0            // array buffer offset
                              );
        
        // 2nd attribute buffer : colors
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, colorbuffer);
        glVertexAttribPointer(
                              1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
                              3,                                // size
                              GL_FLOAT,                         // type
                              GL_FALSE,                         // normalized?
                              0,                                // stride
                              (void*)0                          // array buffer offset
                              );
        
        // Draw the triangle !
        glPointSize(10.0);
        glDrawArrays(GL_POINTS, 0, vertexBufferData.size()); // 12*3 indices starting at 0 -> 12 triangles
        
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        
        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
        
    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
          glfwWindowShouldClose(window) == 0 );
    
    // Cleanup VBO and shader
    glDeleteBuffers(1, &vertexbuffer);
    glDeleteBuffers(1, &colorbuffer);
    glDeleteProgram(programID);
    glDeleteVertexArrays(1, &VertexArrayID);
    
    // Close OpenGL window and terminate GLFW
    glfwTerminate();
    
    return 0;
}

