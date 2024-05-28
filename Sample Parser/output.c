/***************************************************************
                           texture.c
                             
Outputs the parsed data to a file
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "main.h"
#include "texture.h"
#include "mesh.h"
#include "animation.h"
#include "dlist.h"
#include "opengl.h"

#define STRBUF_SIZE 512

typedef struct {
    uint16_t header;
    uint16_t count_meshes;
    uint16_t count_anims;
    uint32_t offset_meshes;
    uint32_t offset_anims;
} BinFile;

typedef struct {
    uint32_t meshdata_offset;
    uint32_t meshdata_size;
    uint32_t vertdata_offset;
    uint32_t vertdata_size;
    uint32_t dldata_offset;
    uint32_t dldata_size;
    uint32_t dldata_slotcount;
} BinFile_TOC_Meshes;

typedef struct {
    int16_t parent;
    uint8_t is_billboard;
    char*   name;
} BinFile_MeshData;

typedef struct {
    int16_t  pos[3];
    uint16_t pad;
    int16_t  tex[2];
    uint8_t  colornormal[4];
} BinFile_UltraVert;

typedef struct {
    float pos[3];
    float tex[2];
    float normal[3];
    float color[3];
} BinFile_DragonVert;


/*==============================
    write_output_text
    Writes the output to a text file
==============================*/

void write_output_text()
{
    int i, count;
    FILE* fp;
    FILE* fp_temp;
    listNode* curnode;
    int tempc;
    char strbuff[STRBUF_SIZE];
    int longestmeshname = 0, longestanimname = 0;
    char makestructs = (list_animations.size > 0 || list_meshes.size > 1);

    // Open the file
    sprintf(strbuff, "%s.h", global_outputname);
    fp = fopen(strbuff, "w+");
    if (fp == NULL)
        terminate("Error: Unable to open file for writing\n");
        
    // Find the mesh and animation with the longest name
    for (curnode = list_meshes.head; curnode != NULL; curnode = curnode->next)
    {
        s64Mesh* mesh = (s64Mesh*)curnode->data;
        int len = strlen(mesh->name);
        if (len > longestmeshname)
            longestmeshname = len;
    }
    for (curnode = list_animations.head; curnode != NULL; curnode = curnode->next)
    {
        s64Anim* anim = (s64Anim*)curnode->data;
        int len = strlen(anim->name);
        if (len > longestanimname)
            longestanimname = len;
    }
        
    // Print the header
    fprintf(fp, "// Generated by "PROGRAM_NAME" V"PROGRAM_VERSION"\n");
    fprintf(fp, "// By Buu342\n\n");
    if (makestructs)
    {
        fprintf(fp, "// Model convenience macro\n");
        fprintf(fp, "#define MODEL_%s (&mdl_%s)\n\n", global_modelname, global_modelname);
    }
    
    // Create some helper macros
    if (makestructs)
    {
        // Iterate through all the meshes and print their names
        count = 0;
        fprintf(fp, "// Mesh data\n#define MESHCOUNT_%s %d\n\n", global_modelname, list_meshes.size);
        for (curnode = list_meshes.head; curnode != NULL; curnode = curnode->next)
        {
            s64Mesh* mesh = (s64Mesh*)curnode->data;
            int nspaces = strlen(mesh->name);
            fprintf(fp, "#define MESH_%s_%s ", global_modelname, mesh->name);
            for (i=nspaces; i<longestmeshname; i++) fputc(' ', fp);
            fprintf(fp, "%d\n", count++);
        }
        fputs("\n", fp);
        
        // Iterate through all the animations and print their names
        count = 0;
        fprintf(fp, "// Animation data\n#define ANIMATIONCOUNT_%s %d\n\n", global_modelname, list_animations.size);
        for (curnode = list_animations.head; curnode != NULL; curnode = curnode->next)
        {
            s64Anim* anim = (s64Anim*)curnode->data;
            int nspaces = strlen(anim->name);
            fprintf(fp, "#define ANIMATION_%s_%s ", global_modelname, anim->name);
            for (i=nspaces; i<longestanimname; i++) fputc(' ', fp);
            fprintf(fp, "%d\n", count++);
        }
        fputs("\n", fp);
    }

    // Construct a text display list
    if (!global_opengl)
        construct_dltext();
    else
        construct_opengl();
    
    // Dump our temporary file into our final file, and then remove it after we're done
    sprintf(strbuff, "temp_%s", global_outputname);
    fp_temp = fopen(strbuff, "r+");
    if (fp_temp == NULL)
        terminate("Error: Unable to open temporary file for reading\n");
    while ((tempc = fgetc(fp_temp)) != EOF)
       fputc(tempc, fp);
    fclose(fp_temp);
    remove(strbuff);
    
    // Write the animation data
    if (list_animations.size > 0)
    {
        fputs("\n", fp);
        fputs("/*********************************\n"
              "          Animation Data\n"
              "*********************************/", fp);
        for (curnode = list_animations.head; curnode != NULL; curnode = curnode->next)
        {
            listNode* keyfnode;
            s64Anim* anim = (s64Anim*)curnode->data;
            fputs("\n\n", fp);
            
            // Print an array of framedata
            for (keyfnode = anim->keyframes.head; keyfnode != NULL; keyfnode = keyfnode->next)
            {
                listNode* meshnode;
                s64Keyframe* keyf = (s64Keyframe*)keyfnode->data;
                fprintf(fp, "static s64Transform anim_%s_%s_framedata%d[] = {\n", global_modelname, anim->name, keyf->keyframe);
                for (meshnode = list_meshes.head; meshnode != NULL; meshnode = meshnode->next) // Iterating meshes because they can be out of order to the frame data, due to texture sorting optimization
                {
                    listNode* fdatanode;
                    for (fdatanode = keyf->framedata.head; fdatanode != NULL; fdatanode = fdatanode->next)
                    {
                        s64Transform* fdata = (s64Transform*)fdatanode->data;
                        if (meshnode->data == fdata->mesh)
                        {
                            fprintf(fp, "    {{%.4ff, %.4ff, %.4ff}, {%.4ff, %.4ff, %.4ff, %.4ff}, {%.4ff, %.4ff, %.4ff}},\n", 
                                fdata->translation.x, fdata->translation.y, fdata->translation.z,
                                fdata->rotation.w, fdata->rotation.x, fdata->rotation.y, fdata->rotation.z,
                                fdata->scale.x, fdata->scale.y, fdata->scale.z
                            );
                            break;
                        }
                    }
                }
                fprintf(fp, "};\n");
            }
            
            // Then print an array of keyframes
            fprintf(fp, "static s64KeyFrame anim_%s_%s_keyframes[] = {\n", global_modelname, anim->name);
            for (keyfnode = anim->keyframes.head; keyfnode != NULL; keyfnode = keyfnode->next)
            {
                s64Keyframe* keyf = (s64Keyframe*)keyfnode->data;
                fprintf(fp, "    {%d, anim_%s_%s_framedata%d},\n", keyf->keyframe, global_modelname, anim->name, keyf->keyframe);
            }
            fprintf(fp, "};");
        }
    }
    
    // Finally, print the Sausage64 structs
    if (makestructs)
    {
        bool ismultimesh = (list_meshes.size > 1);
        
        // Struct comment header
        fputs("\n\n\n", fp);
        fputs("/*********************************\n"
              "        Sausage64 Structs\n"
              "*********************************/\n", fp);
        fputs("\n", fp);
        
        // Mesh list
        fprintf(fp, "static s64Mesh meshes_%s[] = {\n", global_modelname);
        for (curnode = list_meshes.head; curnode != NULL; curnode = curnode->next)
        {
            bool billboard = FALSE;
            s64Mesh* mesh = (s64Mesh*)curnode->data;
            
            // Write the model data line
            fprintf(fp, "    {\"%s\", %d, ", mesh->name, has_property(mesh, "Billboard"));
            if (global_opengl)
                fputs("&", fp);
            if (ismultimesh)
                fprintf(fp, "gfx_%s_%s, ", global_modelname, mesh->name);
            else
                fprintf(fp, "gfx_%s, ", global_modelname);
            if (mesh->parent != NULL)
            {
                int index = 0;
                listNode* pnode;

                for (pnode = list_meshes.head; pnode != NULL; pnode = pnode->next)
                {
                    s64Mesh* parent = (s64Mesh*)pnode->data;
                    if (!strcmp(parent->name, mesh->parent))
                    {
                        fprintf(fp, "%d", index);
                        break;
                    }
                    index++;
                }
            }
            else
                fprintf(fp, "-1");

            fprintf(fp, "},\n");
        }
        fputs("};\n\n", fp);
        
        // Animation list
        fprintf(fp, "static s64Animation anims_%s[] = {\n", global_modelname);
        for (curnode = list_animations.head; curnode != NULL; curnode = curnode->next)
        {
            s64Anim* anim = (s64Anim*)curnode->data;
            fprintf(fp, "    {\"%s\", %d, anim_%s_%s_keyframes},\n", anim->name, anim->keyframes.size, global_modelname, anim->name);
        }
        fputs("};\n\n", fp);

        // Final model data
        fprintf(fp, "static s64ModelData mdl_%s = {%d, %d, meshes_%s, anims_%s};", global_modelname, list_meshes.size, list_animations.size, global_modelname, global_modelname);
    }
    
    // Finish
    if (!global_quiet) printf("Wrote output to '%s.h'\n", global_outputname);
    fclose(fp);
}


/*==============================
    write_output_binary
    Writes the output to a binary file.
==============================*/

void write_output_binary()
{
    int i;
    FILE* fp;
    listNode* curnode;
    char strbuff[STRBUF_SIZE];
    BinFile bin;
    BinFile_TOC_Meshes* toc_meshes;
    BinFile_MeshData* meshdatas;
    void* vertdatas;
    
    // Open the file
    sprintf(strbuff, "%s.bin", global_outputname);
    fp = fopen(strbuff, "wb+");
    if (fp == NULL)
        terminate("Error: Unable to open file for writing\n");
    
    // Generate the file header
    bin.header        = swap_endian16(0x5364);
    bin.count_meshes  = swap_endian16(list_meshes.size);
    bin.count_anims   = swap_endian16(list_animations.size);

    // Malloc stuff
    toc_meshes = (BinFile_TOC_Meshes*)malloc(sizeof(BinFile_TOC_Meshes)*list_meshes.size);
    if (toc_meshes == NULL)
        terminate("Error: Unable to malloc for TOC_Meshes\n");
    meshdatas = (BinFile_MeshData*)malloc(sizeof(BinFile_MeshData)*list_meshes.size);
    if (meshdatas == NULL)
        terminate("Error: Unable to malloc for MeshData\n");
    if (!global_opengl)
        vertdatas = (BinFile_UltraVert**)malloc(sizeof(BinFile_UltraVert*)*list_meshes.size);
    else
        vertdatas = (BinFile_DragonVert**)malloc(sizeof(BinFile_DragonVert*)*list_meshes.size);
    if (vertdatas == NULL)
        terminate("Error: Unable to malloc for Verts\n");

    // Generate the mesh data
    i = 0;
    for (curnode = list_meshes.head; curnode != NULL; curnode = curnode->next)
    {
        int parent = 0;
        s64Mesh* mesh = (s64Mesh*)curnode->data;

        // Find the parent mesh
        if (mesh->parent != NULL)
        {
            listNode* pnode;
            for (pnode = list_meshes.head; pnode != NULL; pnode = pnode->next)
            {
                s64Mesh* p = (s64Mesh*)pnode->data;
                if (!strcmp(p->name, mesh->parent))
                    break;
                parent++;
            }
        }
        else
            parent = -1;

        // Assign the meshdata
        meshdatas[i].parent = parent;
        meshdatas[i].is_billboard = has_property(mesh, "Billboard");
        meshdatas[i].name = mesh->name;

        // Update the mesh data size
        toc_meshes[i].meshdata_size = sizeof(BinFile_MeshData) - sizeof(char*) + strlen(meshdatas[i].name)+1;

        // Create the vert data
        if (!global_opengl)
        {
            int j = 0;
            listNode* vertnode;
            ((BinFile_UltraVert**)vertdatas)[i] = (BinFile_UltraVert*)malloc(sizeof(BinFile_UltraVert)*mesh->verts.size);
            // TODO: Test Malloc

            // Copy the vert data
            // TODO: Check if G_LIGHTING is enabled. If it isn't, then copy vert colors instead of normals 
            for (vertnode = mesh->verts.head; vertnode != NULL; vertnode = vertnode->next)
            {
                s64Vert* vert = (s64Vert*)vertnode->data;
                ((BinFile_UltraVert**)vertdatas)[i][j].pos[0] = vert->pos.x;
                ((BinFile_UltraVert**)vertdatas)[i][j].pos[1] = vert->pos.y;
                ((BinFile_UltraVert**)vertdatas)[i][j].pos[2] = vert->pos.z;
                ((BinFile_UltraVert**)vertdatas)[i][j].pad = 0;
                ((BinFile_UltraVert**)vertdatas)[i][j].tex[0] = vert->UV.x;
                ((BinFile_UltraVert**)vertdatas)[i][j].tex[1] = vert->UV.y;
                ((BinFile_UltraVert**)vertdatas)[i][j].colornormal[0] = vert->normal.x;
                ((BinFile_UltraVert**)vertdatas)[i][j].colornormal[1] = vert->normal.y;
                ((BinFile_UltraVert**)vertdatas)[i][j].colornormal[2] = vert->normal.z;
                ((BinFile_UltraVert**)vertdatas)[i][j].colornormal[3] = 0;
                j++;
            }

            // Update the vert data size in the TOC
            toc_meshes[i].vertdata_size = sizeof(BinFile_UltraVert)*mesh->verts.size;
        }
        else
        {
            int j = 0;
            listNode* vertnode;
            ((BinFile_DragonVert**)vertdatas)[i] = (BinFile_DragonVert*)malloc(sizeof(BinFile_DragonVert)*mesh->verts.size);
            // TODO: Test Malloc

            // Copy the vert data
            for (vertnode = mesh->verts.head; vertnode != NULL; vertnode = vertnode->next)
            {
                s64Vert* vert = (s64Vert*)vertnode->data;
                ((BinFile_DragonVert**)vertdatas)[i][j].pos[0] = vert->pos.x;
                ((BinFile_DragonVert**)vertdatas)[i][j].pos[1] = vert->pos.y;
                ((BinFile_DragonVert**)vertdatas)[i][j].pos[2] = vert->pos.z;
                ((BinFile_DragonVert**)vertdatas)[i][j].tex[0] = vert->UV.x;
                ((BinFile_DragonVert**)vertdatas)[i][j].tex[1] = vert->UV.y;
                ((BinFile_DragonVert**)vertdatas)[i][j].normal[0] = vert->normal.x;
                ((BinFile_DragonVert**)vertdatas)[i][j].normal[1] = vert->normal.y;
                ((BinFile_DragonVert**)vertdatas)[i][j].normal[2] = vert->normal.z;
                ((BinFile_DragonVert**)vertdatas)[i][j].color[0] = vert->color.x;
                ((BinFile_DragonVert**)vertdatas)[i][j].color[1] = vert->color.y;
                ((BinFile_DragonVert**)vertdatas)[i][j].color[2] = vert->color.z;
                j++;
            }

            // Update the vert data size in the TOC
            toc_meshes[i].vertdata_size = sizeof(BinFile_DragonVert)*mesh->verts.size;
        }

        // Create the display list
        if (!global_opengl)
        {
            linkedList* binarydl = dlist_frommesh(mesh, TRUE);
            free(binarydl);
        }
        else
        {

        }

        // Done
        i++;
    }

    // Write the file header
    bin.offset_meshes = swap_endian32(0);
    bin.offset_anims  = swap_endian32(0);
    fwrite(&bin, sizeof(BinFile), 1, fp);
        
    if (!global_quiet) printf("Wrote output to '%s.bin' and '%s.h'\n", global_outputname, global_outputname);
    fclose(fp);
}