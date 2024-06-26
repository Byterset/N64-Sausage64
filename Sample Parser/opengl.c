/***************************************************************
                            opengl.c
                             
Constructs an OpenGL command list string for outputting 
later.
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "main.h"
#include "mesh.h"
#include "opengl.h"


/*********************************
              Macros
*********************************/

#define STRBUF_SIZE 512


/*==============================
    generate_opengl_vcachelist
    Constructs an OpenGL vertex cache list for
    dumping a s64RenderBlock
    @param  The mesh to generate the vcache list of
    @return The generated list of vcache blocks
==============================*/

linkedList* generate_opengl_vcachelist(s64Mesh* mesh)
{
    linkedList* list_vcacherender = list_new();
    n64Material* lastMaterial = NULL;
    VCacheRenderBlock* lastrenderblock = NULL;
    int vertcount = 0, vertoffset = 0;
    int facecount = 0, faceoffset = 0;
    int minvert = INT_MAX, maxvert = 0;
    
    // First, cycle through the vertex cache list and register the material switches
    for (listNode* vcachenode = mesh->vertcache.head; vcachenode != NULL; vcachenode = vcachenode->next)
    {
        vertCache* vcache = (vertCache*)vcachenode->data;
        for (listNode* vertnode = vcache->faces.head; vertnode != NULL; vertnode = vertnode->next)
        {
            int i;
            s64Face* face = (s64Face*)vertnode->data;

            // If a material switch happened, register it
            if (face->material != lastMaterial)
            {
                lastMaterial = face->material;
                VCacheRenderBlock* vcrb = calloc(1, sizeof(VCacheRenderBlock));
                if (vcrb == NULL)
                    terminate("Error: Unable to allocate memory for VCache Render Block\n");
                list_append(list_vcacherender, vcrb);
                if (face->material->type == TYPE_OMIT || face->material->dontload)
                    vcrb->mat = NULL;
                else
                    vcrb->mat = face->material;
                vcrb->matid = get_validmatindex(&list_materials, face->material->name);
                faceoffset = facecount;
                if (lastrenderblock == NULL)
                    vcrb->vertoffset = 0;
                else
                    vcrb->vertoffset = lastrenderblock->vertoffset + lastrenderblock->vertcount;
                lastrenderblock = vcrb;
                minvert = INT_MAX;
                maxvert = 0;
            }

            // Get the min and max vert 
            for (i=0; i<3; i++)
            {
                int vertindex = vertcount + list_index_from_data(&vcache->verts, face->verts[i]);
                if (vertindex < minvert)
                    minvert = vertindex;
                if (vertindex > maxvert)
                    maxvert = vertindex;
            }

            // Calculate the face count + offset
            facecount++;
            lastrenderblock->facecount = facecount - faceoffset;
            lastrenderblock->faceoffset = faceoffset;
            lastrenderblock->vertcount = maxvert - minvert + 1;
        }
        vertcount += vcache->verts.size;
    }
    return list_vcacherender;
}


/*==============================
    construct_opengl
    Constructs an OpenGL Command List 
    and stores it in a temporary file
==============================*/

void construct_opengl()
{
    FILE* fp;
    char strbuff[STRBUF_SIZE];
    bool ismultimesh = (list_meshes.size > 1);
    
    // Open a temp file to write our opengl command list to
    sprintf(strbuff, "temp_%s", global_outputname);
    fp = fopen(strbuff, "w+");
    if (fp == NULL)
        terminate("Error: Unable to open temporary file for writing\n");
        
    // Material data header
    fprintf(fp, "\n/*********************************\n"
                "             Materials\n"
                "*********************************/\n\n"
    );
    
    // Generate the s64Material struct for each material
    for (listNode* matnode = list_materials.head; matnode != NULL; matnode = matnode->next)
    {
        n64Material* mat = (n64Material*)matnode->data;
        
        if (mat->type != TYPE_OMIT && !mat->dontload)
        {
            // Generate the material data
            switch (mat->type)
            {
                case TYPE_TEXTURE:
                    fprintf(fp, "static s64Texture matdata_%s = {&%s, %d, %d, ", mat->name, mat->name, mat->data.image.w, mat->data.image.h);
                    if (!strcmp(mat->texfilter, "G_TF_POINT"))
                        fputs("GL_NEAREST, ", fp);
                    else
                        fputs("GL_LINEAR, ", fp);
                    if (!strcmp(mat->data.image.texmodes, "G_TX_MIRROR"))
                        fputs("GL_MIRRORED_REPEAT_ARB, ", fp);
                    else if (!strcmp(mat->data.image.texmodes, "G_TX_WRAP"))
                        fputs("GL_REPEAT, ", fp);
                    else
                        fputs("GL_CLAMP, ", fp);
                    if (!strcmp(mat->data.image.texmodet, "G_TX_MIRROR"))
                        fputs("GL_MIRRORED_REPEAT_ARB};\n", fp);
                    else if (!strcmp(mat->data.image.texmodet, "G_TX_WRAP"))
                        fputs("GL_REPEAT};\n", fp);
                    else
                        fputs("GL_CLAMP};\n", fp);
                    break;
                case TYPE_PRIMCOL:
                    fprintf(fp, "static s64PrimColor matdata_%s = {%d, %d, %d, %d};\n", mat->name, mat->data.color.r, mat->data.color.g, mat->data.color.b, 255/*mat->data.color.a*/);
                    break;
            }
            
            // And then the material itself
            fprintf(fp, "static s64Material mat_%s = {", mat->name);
            switch (mat->type)
            {
                case TYPE_TEXTURE:
                    fprintf(fp, "TYPE_TEXTURE, ");
                    break;
                case TYPE_PRIMCOL:
                    fprintf(fp, "TYPE_PRIMCOL, ");
                    break;
            }
            fprintf(fp, "&matdata_%s, %d, %d, %d, %d, %d};\n\n", mat->name, 
                mat_hasgeoflag(mat, "G_LIGHTING"), mat_hasgeoflag(mat, "G_CULL_FRONT"), mat_hasgeoflag(mat, "G_CULL_BACK"),
                mat_hasgeoflag(mat, "G_SHADING_SMOOTH"), mat_hasgeoflag(mat, "G_ZBUFFER")
            );
        }
    }
    
        
    // Vertex data header
    fprintf(fp, "\n/*********************************\n"
                "              Models\n"
                "*********************************/\n\n"
    );
        
    // Iterate through all the meshes
    for (listNode* meshnode = list_meshes.head; meshnode != NULL; meshnode = meshnode->next)
    {
        s64Mesh* mesh = (s64Mesh*)meshnode->data;
        int faceindex = 0, vertindex = 0;
        linkedList* list_vcacherender = generate_opengl_vcachelist(mesh);
        
        // Cycle through the vertex cache list and dump the vertices
        vertindex = 0;
        fprintf(fp, "static f32 vtx_%s", global_modelname);
        if (ismultimesh)
            fprintf(fp, "_%s", mesh->name);
        fprintf(fp, "[][11] = {\n");
        for (listNode* vcachenode = mesh->vertcache.head; vcachenode != NULL; vcachenode = vcachenode->next)
        {
            vertCache* vcache = (vertCache*)vcachenode->data;
            
            // Cycle through all the verts
            for (listNode* vertnode = vcache->verts.head; vertnode != NULL; vertnode = vertnode->next)
            {
                s64Vert* vert = (s64Vert*)vertnode->data;
                
                // Dump the vert data
                fprintf(fp, "    {%.4ff, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff, %.4ff}, /* %d */\n", 
                    vert->pos.x, vert->pos.y, vert->pos.z,
                    vert->UV.x, vert->UV.y,
                    vert->normal.x, vert->normal.y, vert->normal.z,
                    vert->color.x, vert->color.y, vert->color.z,
                    vertindex++
                );
            }
        }
        fprintf(fp, "};\n\n");
        
        // Then cycle through the vertex cache list again, but now dump the faces
        fprintf(fp, "static u16 ind_%s", global_modelname);
        if (ismultimesh)
            fprintf(fp, "_%s", mesh->name);
        fprintf(fp, "[][3] = {\n");
        vertindex = 0;
        faceindex = 0;
        for (listNode* vcachenode = mesh->vertcache.head; vcachenode != NULL; vcachenode = vcachenode->next)
        {
            vertCache* vcache = (vertCache*)vcachenode->data;
            
            // Cycle through all the faces
            for (listNode* vertnode = vcache->faces.head; vertnode != NULL; vertnode = vertnode->next)
            {
                s64Face* face = (s64Face*)vertnode->data;
                
                // Dump the face data
                fprintf(fp, "    {%u, %u, %u}, /* %d */\n", 
                    vertindex + list_index_from_data(&vcache->verts, face->verts[0]), 
                    vertindex + list_index_from_data(&vcache->verts, face->verts[1]), 
                    vertindex + list_index_from_data(&vcache->verts, face->verts[2]),
                    faceindex++
                );
            }
            vertindex += vcache->verts.size;
        }
        fprintf(fp, "};\n\n");
        
        // Next, generate the render blocks
        fprintf(fp, "static s64RenderBlock renb_%s", global_modelname);
        if (ismultimesh)
            fprintf(fp, "_%s", mesh->name);
        fprintf(fp, "[] = {\n");
        for (listNode* vcachenode = list_vcacherender->head; vcachenode != NULL; vcachenode = vcachenode->next)
        {
            VCacheRenderBlock* vcacheb = (VCacheRenderBlock*)vcachenode->data;
            fprintf(fp, "\t{");
            fprintf(fp, "&vtx_%s", global_modelname);
            if (ismultimesh)
                fprintf(fp, "_%s", mesh->name);
            fprintf(fp, "[%d], %d, %d, ", vcacheb->vertoffset, vcacheb->vertcount, vcacheb->facecount);
            fprintf(fp, "&ind_%s", global_modelname);
            if (ismultimesh)
                fprintf(fp, "_%s", mesh->name);
            if (vcacheb->mat != NULL)
                fprintf(fp, "[%d], &mat_%s},\n", vcacheb->faceoffset, vcacheb->mat->name);
            else
                fprintf(fp, "[%d], NULL},\n", vcacheb->faceoffset);
        }
        fprintf(fp, "};\n\n");
        
        // Finally, generate the "Display List"
        fprintf(fp, "static s64Gfx gfx_%s", global_modelname);
        if (ismultimesh)
            fprintf(fp, "_%s", mesh->name);
        fprintf(fp, " = {%d, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, ", list_vcacherender->size);
        fprintf(fp, "renb_%s", global_modelname);
        if (ismultimesh)
            fprintf(fp, "_%s", mesh->name);
        fprintf(fp, "};\n\n");
        
        // Cleanup
        list_destroy_deep(list_vcacherender);
        free(list_vcacherender);
    }
    
    // State we finished
    if (!global_quiet) printf("Finish building display lists\n");
    fclose(fp);
}