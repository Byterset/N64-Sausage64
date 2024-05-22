/***************************************************************
                           texture.c
                             
Outputs the parsed data to a file
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "texture.h"
#include "mesh.h"
#include "animation.h"

#define STRBUF_SIZE 512

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
    fp = fopen(global_outputname, "w+");
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
    if (!global_quiet) printf("Wrote output to '%s'\n", global_outputname);
    fclose(fp);
}


/*==============================
    write_output_binary
    NOT IMPLEMENTED!
    Writes the output to a binary file.
==============================*/

void write_output_binary()
{
    FILE* fp;
    
    // Open the file
    fp = fopen(global_outputname, "wb+");
    if (fp == NULL)
        terminate("Error: Unable to open file for writing\n");
    
    // TODO
        
    if (!global_quiet) printf("Wrote output to '%s'", global_outputname);
    fclose(fp);
}