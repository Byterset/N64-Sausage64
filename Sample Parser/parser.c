/***************************************************************
                            parser.c
                             
Very basic Sausage64 file parser. The parser does little to no 
error checking, expects things to be properly formatted, and has
not properly implemented how comments are handled (it expects 
comments to always only take up a single line). For files
generated exactly from Blender, that's not a big deal.
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "parser.h"
#include "mesh.h"
#include "animation.h"


/*********************************
              Macros
*********************************/

#define STRBUFF_SIZE 512


/*********************************
             Globals
*********************************/

// Lexer state
static lexState lexer_curstate = STATE_NONE;
static lexState lexer_prevstate = STATE_NONE;


/*==============================
    lexer_changestate
    Changes the current lexer state
    @param The new lexer state
==============================*/

static inline void lexer_changestate(lexState state)
{
    lexer_prevstate = lexer_curstate;
    lexer_curstate = state;
}


/*==============================
    lexer_restorestate
    Restores the lexer state to its previous value
==============================*/

static inline void lexer_restorestate()
{
    if (lexer_curstate == lexer_prevstate)
        lexer_prevstate = STATE_NONE;
    lexer_curstate = lexer_prevstate;
}


/*==============================
    parse_sausage
    Parses a sausage64 model file
    @param The pointer to the .s64 file's handle
==============================*/

void parse_sausage(FILE* fp)
{
    int vertcount;
    listNode* curnode;
    s64Mesh* curmesh;
    s64Vert* curvert;
    s64Face* prevface;
    s64Face* curface;
    s64Anim* curanim;
    s64Keyframe* curkeyframe;
    s64Transform* curframedata;
    n64Material* curmat;
    Vector3D tempvec;
    
    if (!global_quiet) printf("Parsing s64 model\n");
    
    // Read the file until we reached the end
    while (!feof(fp))
    {
        char strbuf[STRBUFF_SIZE];
        char* strdata;
        
        // Read a string from the text file
        if (fgets(strbuf, STRBUFF_SIZE, fp) == NULL && !feof(fp))
            terminate("Error: Problem reading s64 file\n");
        
        // Split the string by spaces
        strdata = strtok(strbuf, " ");
        
        // Parse each substring
        do
        {
            // Handle C comment lines
            if (strstr(strdata, "//") != NULL)
                break;
                
            // Handle C comment block starting
            if (strstr(strdata, "/*") != NULL)
            {
                lexer_changestate(STATE_COMMENTBLOCK);
                break;
            }
                
            // Handle C comment blocks
            if (lexer_curstate == STATE_COMMENTBLOCK)
            {
                if (strstr(strdata, "*/") != NULL)
                    lexer_restorestate();
                continue;
            }
            
            // Handle Begin
            if (!strcmp(strdata, "BEGIN"))
            {
                // Handle the next substring
                strdata = strtok(NULL, " ");
                switch (lexer_curstate)
                {
                    case STATE_MESH:
                        strdata[strcspn(strdata, "\r\n")] = 0;
                        if (!strcmp(strdata, "VERTICES"))
                            lexer_changestate(STATE_VERTICES);
                        else if (!strcmp(strdata, "FACES"))
                            lexer_changestate(STATE_FACES);
                        break;
                    case STATE_ANIMATION:
                        if (!strcmp(strdata, "KEYFRAME"))
                        {
                            lexer_changestate(STATE_KEYFRAME);
                            curkeyframe = add_keyframe(curanim, atoi(strtok(NULL, " ")));
                        }
                        break;
                    case STATE_NONE:
                        if (!strcmp(strdata, "MESH"))
                        {
                            lexer_changestate(STATE_MESH);
                            
                            // Get the mesh name and fix the trailing newline
                            strdata = strtok(NULL, " ");
                            strdata[strcspn(strdata, "\r\n")] = 0;
                            
                            // Create the mesh
                            curmesh = add_mesh(strdata);
                            if (!global_quiet) printf("    Created new mesh '%s'\n", strdata);
                        }
                        else if (!strcmp(strdata, "ANIMATION"))
                        {
                            lexer_changestate(STATE_ANIMATION);
                            
                            // Get the animation name and fix the trailing newline
                            strdata = strtok(NULL, " ");
                            strdata[strcspn(strdata, "\r\n")] = 0;
                            
                            // Create the animation
                            curanim = add_animation(strdata);
                            if (!global_quiet) printf("    Created new animation '%s'\n", strdata);
                        }
                        break;
                }
            }
            else if (!strcmp(strdata, "END")) // Handle End
            {
                lexer_restorestate();
            }
            else // Handle the rest
            {
                listNode* mmat = NULL;
                switch (lexer_curstate)
                {
                    case STATE_MESH:
                        if (!strcmp(strdata, "ROOT"))
                        {
                            tempvec.x = atof(strtok(NULL, " "));
                            tempvec.y = atof(strtok(NULL, " "));
                            tempvec.z = atof(strtok(NULL, " "));
                            curmesh->root = tempvec;
                        }
                        if (!strcmp(strdata, "PARENT"))
                        {
                            strdata = strtok(NULL, " ");
                            strdata[strcspn(strdata, "\r\n")] = 0;
                            curmesh->parent = (char*)calloc(strlen(strdata)+1, 1);
                            strcpy(curmesh->parent, strdata);
                        }
                        if (!strcmp(strdata, "PROPERTIES"))
                        {
                            while ((strdata = strtok(NULL, " ")) != NULL)
                            {
                                char* prop = (char*)calloc(strlen(strdata)+1, 1);
                                strdata[strcspn(strdata, "\r\n")] = 0;
                                strcpy(prop, strdata);
                                list_append(&curmesh->props, prop);
                            }
                        }
                        break;
                    case STATE_VERTICES:
                        curvert = add_vertex(curmesh);
                        
                        // Set the vertex data
                        curvert->pos.x = atof(strdata);
                        curvert->pos.y = atof(strtok(NULL, " "));
                        curvert->pos.z = atof(strtok(NULL, " "));
                        curvert->normal.x = atof(strtok(NULL, " "));
                        curvert->normal.y = atof(strtok(NULL, " "));
                        curvert->normal.z = atof(strtok(NULL, " "));
                        curvert->color.x = atof(strtok(NULL, " "));
                        curvert->color.y = atof(strtok(NULL, " "));
                        curvert->color.z = atof(strtok(NULL, " "));
                        curvert->UV.x = atof(strtok(NULL, " "));
                        curvert->UV.y = atof(strtok(NULL, " "));
                        break;
                    case STATE_FACES:
                        curface = add_face(curmesh);
                        
                        // Set the face data
                        vertcount = atoi(strdata);
                        if (vertcount > 4)
                            terminate("Error: This tool does not support faces with more than 4 vertices\n");
                        curface->verts[0] = find_vert(curmesh, atof(strtok(NULL, " ")));
                        curface->verts[1] = find_vert(curmesh, atof(strtok(NULL, " ")));
                        curface->verts[2] = find_vert(curmesh, atof(strtok(NULL, " ")));
                            
                        // Handle quads
                        prevface = NULL;
                        if (vertcount == 4)
                        {
                            prevface = curface;
                            curface = add_face(curmesh);
                            curface->verts[0] = prevface->verts[0];
                            curface->verts[1] = prevface->verts[2];
                            curface->verts[2] = find_vert(curmesh, atof(strtok(NULL, " ")));
                        }
                            
                        // Get the material name and check if it exists already
                        strdata = strtok(NULL, " ");
                        strdata[strcspn(strdata, "\r\n")] = 0;
                        curmat = find_material(strdata);
                        if (curmat == NULL && strcmp(strdata, "None") != 0)
                            curmat = request_material(strdata);
                        curface->material = curmat;
                        
                        // Assign the face to the previous face as well, if we have a quad
                        if (prevface != NULL)
                        {
                            prevface->material = curmat;
                            prevface = NULL;
                        }
                        
                        // Check if this material name has been added to this mesh's material list
                        for (mmat = curmesh->materials.head; mmat != NULL; mmat = mmat->next)
                            if (!strcmp(((n64Material*)mmat->data)->name,  strdata))
                                break;
                                
                        // If it hasn't been, add it
                        if (mmat == NULL)
                            list_append(&curmesh->materials, curmat);
                        break;
                    case STATE_KEYFRAME:
                        curframedata = add_framedata(curkeyframe);
                        curframedata->mesh = find_mesh(strdata);
                        curframedata->translation.x = atof(strtok(NULL, " "));
                        curframedata->translation.y = atof(strtok(NULL, " "));
                        curframedata->translation.z = atof(strtok(NULL, " "));
                        curframedata->rotation.w = atof(strtok(NULL, " "));
                        curframedata->rotation.x = atof(strtok(NULL, " "));
                        curframedata->rotation.y = atof(strtok(NULL, " "));
                        curframedata->rotation.z = atof(strtok(NULL, " "));
                        curframedata->scale.x = atof(strtok(NULL, " "));
                        curframedata->scale.y = atof(strtok(NULL, " "));
                        curframedata->scale.z = atof(strtok(NULL, " "));
                        break;
                }
            }
        }
        while ((strdata = strtok(NULL, " ")) != NULL);
    }
        
    // Close the file as we're done with it
    if (!global_quiet) printf("Finished parsing s64 model\n    Mesh count: %d\n    Animation count: %d\n    Material count: %d\n", list_meshes.size, list_animations.size, list_materials.size-1);
    fclose(fp);
    
    // Sort the framedata by the order the meshes are in (Note: horrible time complexity as this is a bodge solution)
    for (curnode = list_animations.head; curnode != NULL; curnode = curnode->next)
    {
        listNode* curkeyfnode;
        s64Anim* anim = (s64Anim*)curnode->data;
        
        // Iterate through each keyframe
        for (curkeyfnode = anim->keyframes.head; curkeyfnode != NULL; curkeyfnode = curkeyfnode->next)
        {
            listNode* curmeshnode;
            linkedList correctorder = EMPTY_LINKEDLIST;
            s64Keyframe* keyf = (s64Keyframe*)curkeyfnode->data;
            
            // Go through each mesh
            for (curmeshnode = list_meshes.head; curmeshnode != NULL; curmeshnode = curmeshnode->next)
            {
                listNode* curfdatanode;
                s64Mesh* mesh = (s64Mesh*)curmeshnode->data;
                
                // Compare with each frame data
                for (curfdatanode = keyf->framedata.head; curfdatanode != NULL; curfdatanode = curfdatanode->next)
                {
                    s64Transform* fdata = (s64Transform*)curfdatanode->data;
                    if (fdata->mesh == mesh)
                    {
                        listNode* elem = list_remove(&keyf->framedata, fdata);
                        list_append(&correctorder, elem->data);
                        free(elem);
                        break;
                    }
                }
            }
            keyf->framedata = correctorder;
        }
    }
    
    // Iterate through the animations
    for (curnode = list_animations.head; curnode != NULL; curnode = curnode->next)
    {
        int firstframe = -1;
        listNode* animnode;
        s64Anim* anim = (s64Anim*)curnode->data;
        
        // Iterate through the animations
        for (animnode = anim->keyframes.head; animnode != NULL; animnode = animnode->next)
        {
            listNode* keyfnode;
            s64Keyframe* keyf = (s64Keyframe*)animnode->data;
            if (firstframe == -1 && keyf->keyframe == 0)
                break;
            if (firstframe == -1 && keyf->keyframe != 0)
                firstframe = keyf->keyframe;
            keyf->keyframe -= firstframe;
        }
    }    

    // Fix mesh and animation roots
    if (global_fixroot)
    {
        listNode* datanode;
        
        // Iterate through the meshes
        for (datanode = list_meshes.head; datanode != NULL; datanode = datanode->next)
        {
            listNode* vertnode;
            s64Mesh* mesh = (s64Mesh*)datanode->data;
            
            // Iterate through the vertices
            for (vertnode = mesh->verts.head; vertnode != NULL; vertnode = vertnode->next)
            {
                s64Vert* vert = (s64Vert*)vertnode->data;
                vert->pos.x -= mesh->root.x;
                vert->pos.y -= mesh->root.y;
                vert->pos.z -= mesh->root.z;
            }
        }
        
        // Iterate through the animations
        for (datanode = list_animations.head; datanode != NULL; datanode = datanode->next)
        {
            listNode* animnode;
            s64Anim* anim = (s64Anim*)datanode->data;
            
            // Iterate through the animations
            for (animnode = anim->keyframes.head; animnode != NULL; animnode = animnode->next)
            {
                listNode* keyfnode;
                s64Keyframe* keyf = (s64Keyframe*)animnode->data;
                
                // Iterate through the keyframes
                for (keyfnode = keyf->framedata.head; keyfnode != NULL; keyfnode = keyfnode->next)
                {
                    s64Transform* fdata = (s64Transform*)keyfnode->data;

                    // Fix the translations
                    fdata->translation.x += fdata->mesh->root.x;
                    fdata->translation.y += fdata->mesh->root.y;
                    fdata->translation.z += fdata->mesh->root.z;
                }
            }
        }
        if (!global_quiet) printf("Fixed model and animation roots\n");
    }
}