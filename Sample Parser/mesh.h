#ifndef _SAUSN64_MESH_H
#define _SAUSN64_MESH_H

    #include "material.h"


    /*********************************
                 Structs
    *********************************/
    
    #define MAXVERTS 3

    // Mesh struct
    typedef struct {
        char* name;
        char* parent;
        Vector3D root;
        linkedList verts;
        linkedList faces;
        linkedList materials;
        linkedList props;
        linkedList vertcache;
    } s64Mesh;
    
    // Vertex struct
    typedef struct {
        Vector3D pos;
        Vector3D normal;
        Vector3D color;
        Vector2D UV;
    } s64Vert;
    
    // Face struct
    typedef struct {
        s64Vert* verts[MAXVERTS];
        n64Material* material;
    } s64Face;
    
    // Vertex cache struct
    typedef struct {
        linkedList verts;
        linkedList faces;
    } vertCache;
    
    
    /*********************************
                Functions
    *********************************/
    
    extern s64Mesh*     add_mesh(char* name);
    extern s64Vert*     add_vertex(s64Mesh* mesh);
    extern s64Face*     add_face(s64Mesh* mesh);
    extern s64Mesh*     find_mesh(char* name);
    extern s64Vert*     find_vert(s64Mesh* mesh, int index);
    extern n64Material* find_material_fromvert(linkedList* faces, s64Vert* vert);
    extern bool         has_property(s64Mesh* mesh, char* property);
    
#endif