/***************************************************************
                            dlist.c
                             
Constructs a display list string for outputting later.
***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "main.h"
#include "dlist.h"

/*********************************
              Macros
*********************************/

#define STRBUF_SIZE 512

#define generate(c, ...) (generator(c, commands_f3dex2[c].argcount, ##__VA_ARGS__))


/*********************************
              Globals
*********************************/

// List of commands supported by Arabiki
const DListCName supported_binary[] = {
    DPSetCycleType,
    DPSetRenderMode,
    DPSetCombineMode,
    DPSetTextureFilter,
    SPClearGeometryMode,
    SPSetGeometryMode,
    DPLoadTextureBlock,
    DPLoadTextureBlock_4b,
    DPSetPrimColor,
    SPVertex,
    SP1Triangle,
    SP2Triangles,
    DPPipeSync,
    SPEndDisplayList
}; 

// Global parsing state
n64Material* lastMaterial = NULL;


/*==============================
    swap_endian16
    Swaps the endianess of the data (16-bit)
    @param   The data to swap the endianess of
    @returns The data with endianess swapped
==============================*/

uint16_t swap_endian16(uint16_t val)
{
    return ((val << 8) & 0xFF00) | ((val >> 8) & 0x00FF);
}


/*==============================
    swap_endian32
    Swaps the endianess of the data (32-bit)
    @param   The data to swap the endianess of
    @returns The data with endianess swapped
==============================*/

uint32_t swap_endian32(uint32_t val)
{
    return ((val << 24)) | ((val << 8) & 0x00FF0000) | ((val >> 8) & 0x0000FF00) | ((val >> 24));
}


/*==============================
    swap_endianfloat
    Swaps the endianess of the data (32-bit float)
    @param   The data to swap the endianess of
    @returns The data with endianess swapped
==============================*/

float swap_endianfloat(float val)
{
   float ret;
   uint8_t* swapfrom = (uint8_t*)&val;
   uint8_t* swapto = (uint8_t*)&ret;
   swapto[0] = swapfrom[3];
   swapto[1] = swapfrom[2];
   swapto[2] = swapfrom[1];
   swapto[3] = swapfrom[0];
   return ret;
}


/*==============================
    mallocstring
    Creates a newly malloc'ed copy of a string
    @param   The string to create
    @returns A malloc'ed copy
==============================*/

static char* mallocstring(char* str)
{
    char* ret = (char*)malloc(sizeof(char)*(strlen(str)+1));
    strcpy(ret, str);
    return ret;
}


/*==============================
    _dlist_commandstring
    Creates a static display list command
    string from a dlist command.
    Don't use directly, use the macro
    @param   The display list command name
    @param   The number of arguments
    @param   Variable arguments
    @returns A malloc'ed string
==============================*/

static void* _dlist_commandstring(DListCName c, int size, ...)
{
    va_list args;
    char strbuff[STRBUF_SIZE];
    va_start(args, size);
    sprintf(strbuff, "    gs%s(", commands_f3dex2[c].name);
    for (int i=0; i<size; i++)
    {
        strcat(strbuff, va_arg(args, char*));
        if (i+1 != size)
            strcat(strbuff, ", ");
    }
    strcat(strbuff, "),\n");
    va_end(args);
    return mallocstring(strbuff);
}


/*==============================
    _dlist_commandbinary
    Creates a binary display list command
    from a dlist command.
    Don't use directly, use the macro
    @param   The display list command name
    @param   The number of arguments
    @param   Variable arguments
    @returns A malloc'ed binary block
==============================*/

static void* _dlist_commandbinary(DListCName c, int size, ...)
{
    char supported = 0;
    va_list args;
    //va_list args2;
    DLCBinary* binarydata;
    va_start(args, size);
    //va_copy(args2, args);

    // Check the command is supported
    for (int i=0; i<sizeof(supported_binary)/sizeof(supported_binary[0]); i++)
    {
        if (supported_binary[i] == c)
        {
            supported = 1;
            break;
        }
    }
    if (!supported)
    {
        char strbuff[STRBUF_SIZE];
        sprintf(strbuff, "Unsupported Binary DL command %s", commands_f3dex2[c].name);
        terminate(strbuff);
    }

    // Malloc the binary data
    binarydata = (DLCBinary*)calloc(sizeof(DLCBinary), 1);
    if (binarydata == NULL)
        terminate("Unable to malloc binary data struct");
    binarydata->cmd = c;
    binarydata->size = size;
    switch (c)
    {
        case DPLoadTextureBlock_4b:
        case DPLoadTextureBlock: // Compact into 1 word for texture index, 2 bytes for fmt+siz (siz is zero for _4b), two words for width + height, 7 bytes for everything else
            binarydata->size = 4;
            break;
        case SPVertex: // Compact the offset into 1 word, and the other two arguments into 2 bytes
            binarydata->size = 1;
            break;
        case SP1Triangle: // All arguments for 1Triangle fit in 1 dword
            binarydata->size = 1;
            break;
        case SP2Triangles: // All arguments for 2Triangles fit in 2 dwords
            binarydata->size = 2;
            break;
        case DPSetPrimColor: // Compact Primcolor into 1 dword for l and m, 1 dword for color
            binarydata->size = 2;
            break;
        case DPSetCombineMode: // Each Combine argument is up to 255, so we can compact everything in 4 dwords
            binarydata->size = 4;
            binarydata->cmd = DPSetCombineLERP;
            break;
    }
    binarydata->data = (uint32_t*)calloc(sizeof(uint32_t)*binarydata->size, 1);
    if (binarydata->data  == NULL)
        terminate("Unable to malloc binary data buffer");

    // Go through each argument
    for (int i=0; i<size; i++)
    {
        char* arg = va_arg(args, char*);
        bool parsed = FALSE;

        // Handle edge cases
        switch (c)
        {
            case DPLoadTextureBlock_4b:
            case DPLoadTextureBlock:
                if (i == 0) // First argument is the texture name, we just want the texture index
                {
                    *(((uint16_t*)(&binarydata->data[0]))) = swap_endian16(get_validtexindex(&list_materials, arg));
                }
                else
                {
                    int realindex = i;
                    int argval = 0;
                    if (i >= 2 && c == DPLoadTextureBlock_4b)
                        realindex++;
                    if (realindex == 1 || realindex == 2)
                        ((uint8_t*)binarydata->data)[2+(realindex-1)] = gbi_resolvemacro(arg);
                    else if (realindex == 3 || realindex == 4)
                        ((uint16_t*)binarydata->data)[2+(realindex-3)] = swap_endian16(atoi(arg));
                    else
                    {
                        if (strlen(arg) > 2 && arg[0] == 'G' && arg[1] == '_')
                            argval = gbi_resolvemacro(arg);
                        else
                            argval = atoi(arg);
                        ((uint8_t*)binarydata->data)[8+(realindex-5)] = argval;
                    }
                }
                parsed = TRUE;
                break;
            case SP2Triangles:
            case SP1Triangle:
                *(((uint8_t*)(&binarydata->data[0]))+i) = (uint8_t)atoi(arg);
                parsed = TRUE;
                break;
            case DPSetPrimColor:
                if (i == 0 || i == 1)
                    *(((int16_t*)(&binarydata->data[0]))+i) = swap_endian16(atoi(arg));
                else
                    *(((uint8_t*)(&binarydata->data[1]))+(i-2)) = (uint8_t)atoi(arg);
                parsed = TRUE;
                break;
            case DPSetCombineMode:
                memcpy(&binarydata->data[i*2], gbi_resolveccmode(arg), 8);
                parsed = TRUE;
                break;
            case SPVertex:
                if (i == 0) // First argument is a pointer offset, we just want the offset
                {
                    char* curchar = arg;
                    while (*curchar != '+')
                        curchar++;
                    ((uint16_t*)binarydata->data)[0] = swap_endian16(atoi(curchar+1));
                }
                else
                    ((uint8_t*)binarydata->data)[2+(i-1)] = atoi(arg);
                parsed = TRUE;
                break;
        }

        // Handle everything else (Macro values, ints as strings, and hex values as strings)
        if (!parsed)
        {
            int argval;
            if (strlen(arg) > 2 && arg[0] == 'G' && arg[1] == '_')
                argval = gbi_resolvemacro(arg);
            else if (strlen(arg) > 2 && arg[0] == '0' && arg[1] == 'x')
                argval = strtol(arg, NULL, 16);
            else
                argval = atoi(arg);
            binarydata->data[i] = swap_endian32(argval);
        }
    }
    va_end(args);

    // Debugging printf
    //printf("Turned %s with ", commands_f3dex2[c].name);
    //for (int i=0; i<size; i++)
    //    printf("%s, ", va_arg(args2, char*));
    //printf("into %08x ", swap_endian32(c));
    //for (int i=0; i<binarydata->size; i++)
    //    printf("%08x ", binarydata->data[i]);
    //printf("\n");
    //va_end(args2);

    return binarydata;
}


/*==============================
    dlist_frommesh
    Constructs a display list from a single mesh
    @param   The mesh to build a DL of
    @param   Whether the DL should be binary
    @returns A linked list with the DL data
==============================*/

linkedList* dlist_frommesh(s64Mesh* mesh, char isbinary)
{
    char strbuff[STRBUF_SIZE];
    linkedList* out = list_new();
    bool ismultimesh = (list_meshes.size > 1);
    int vertindex = 0;
    if (out == NULL)
        terminate("Error: Unable to malloc for output list\n");
    void* (*generator)(DListCName c, int size, ...);

    // Select the generation function
    if (isbinary)
        generator = &_dlist_commandbinary;
    else
        generator = &_dlist_commandstring;

    // Loop through the vertex caches
    for (listNode* vcachenode = mesh->vertcache.head; vcachenode != NULL; vcachenode = vcachenode->next)
    {
        vertCache* vcache = (vertCache*)vcachenode->data;
        listNode* prevfacenode = vcache->faces.head;
        bool loadedverts = FALSE;
        
        // Cycle through all the faces
        for (listNode* facenode = vcache->faces.head; facenode != NULL; facenode = facenode->next)
        {
            s64Face* face = (s64Face*)facenode->data;
            n64Material* mat = face->material;
            
            // If we want to skip the initial display list setup, then change the value of our last texture to skip the next if statement
            if (lastMaterial == NULL && !global_initialload)
                lastMaterial = mat;
        
            // If a texture change was detected, load the new texture data
            if (lastMaterial != mat && mat->type != TYPE_OMIT)
            {
                int i;
                bool pipesync = FALSE;
                bool changedgeo = FALSE;
                
                // Check for different cycle type
                if (lastMaterial == NULL || strcmp(mat->cycle, lastMaterial->cycle) != 0)
                {
                    list_append(out, generate(DPSetCycleType, mat->cycle));
                    pipesync = TRUE;
                }
                
                // Check for different render mode
                if (lastMaterial == NULL || strcmp(mat->rendermode1, lastMaterial->rendermode1) != 0 || strcmp(mat->rendermode2, lastMaterial->rendermode2) != 0)
                {
                    list_append(out, generate(DPSetRenderMode, mat->rendermode1, mat->rendermode2));
                    pipesync = TRUE;
                }
                
                // Check for different combine mode
                if (lastMaterial == NULL || strcmp(mat->combinemode1, lastMaterial->combinemode1) != 0 || strcmp(mat->combinemode2, lastMaterial->combinemode2) != 0)
                {
                    list_append(out, generate(DPSetCombineMode, mat->combinemode1, mat->combinemode2));
                    pipesync = TRUE;
                }
                
                // Check for different texture filter
                if (lastMaterial == NULL || strcmp(mat->texfilter, lastMaterial->texfilter) != 0)
                {
                    list_append(out, generate(DPSetTextureFilter, mat->texfilter));
                    pipesync = TRUE;
                }
                
                // Check for different geometry mode
                if (lastMaterial != NULL)
                {
                    int flagcount_old = 0;
                    int flagcount_new = 0;
                    char* flags_old[MAXGEOFLAGS];
                    char* flags_new[MAXGEOFLAGS];

                    // Store the pointer to the flags somewhere to make the iteration easier
                    for (i=0; i<MAXGEOFLAGS; i++)
                    {
                        if (mat->geomode[i][0] != '\0')
                        {
                            flags_new[flagcount_new] = mat->geomode[i];
                            flagcount_new++;
                        }
                        if (lastMaterial->geomode[i][0] != '\0')
                        {
                            flags_old[flagcount_old] = lastMaterial->geomode[i];
                            flagcount_old++;
                        }
                    }

                    // Check if all the flags exist in this other texture
                    if (flagcount_new == flagcount_old)
                    {
                        int j;
                        bool hasthisflag = FALSE;
                        for (i=0; i<flagcount_new; i++)
                        {
                            for (j=0; j<flagcount_old; j++)
                            {
                                if (!strcmp(flags_new[i], flags_old[j]))
                                {
                                    hasthisflag = TRUE;
                                    break;
                                }
                            }
                            if (!hasthisflag)
                            {
                                changedgeo = TRUE;
                                break;
                            }
                        }
                    }
                    else
                        changedgeo = TRUE;
                }
                else
                    changedgeo = TRUE;
                    
                // If a geometry mode flag changed, then update the display list
                if (changedgeo)
                {
                    bool appendline = FALSE;
                
                    // TODO: Smartly omit geometry flags commands based on what changed
                    list_append(out, generate(SPClearGeometryMode, "0xFFFFFFFF"));
                    strbuff[0] = '\0';
                    for (i=0; i<MAXGEOFLAGS; i++)
                    {
                        if (mat->geomode[i][0] == '\0')
                            continue;
                        if (appendline)
                        {
                            strcat(strbuff, " | ");
                            appendline = FALSE;
                        }
                        strcat(strbuff, mat->geomode[i]);
                        appendline = TRUE;
                    }
                    list_append(out, generate(SPSetGeometryMode, strbuff));
                }
                
                // Load the material if it wasn't marked as DONTLOAD
                if (!mat->dontload)
                {
                    char d1[32], d2[32], d3[32], d4[32];
                    if (mat->type == TYPE_TEXTURE)
                    {
                        sprintf(d1, "%d", mat->data.image.w);
                        sprintf(d2, "%d", mat->data.image.h);
                        sprintf(d3, "%d", nearest_pow2(mat->data.image.w));
                        sprintf(d4, "%d", nearest_pow2(mat->data.image.h));
                        if (!strcmp(mat->data.image.colsize, "G_IM_SIZ_4b"))
                        {
                            list_append(out, generate(DPLoadTextureBlock_4b, 
                                mat->name, mat->data.image.coltype, d1, d2, "0",
                                mat->data.image.texmodes, mat->data.image.texmodet, d3, d4, "G_TX_NOLOD", "G_TX_NOLOD")
                            );
                        }
                        else
                        {
                            list_append(out, generate(DPLoadTextureBlock, 
                                mat->name, mat->data.image.coltype, mat->data.image.colsize, d1, d2, "0",
                                mat->data.image.texmodes, mat->data.image.texmodet, d3, d4, "G_TX_NOLOD", "G_TX_NOLOD")
                            );
                        }
                        pipesync = TRUE;
                    }
                    else if (mat->type == TYPE_PRIMCOL)
                    {
                        sprintf(d1, "%d", mat->data.color.r);
                        sprintf(d2, "%d", mat->data.color.g);
                        sprintf(d3, "%d", mat->data.color.b);
                        list_append(out, generate(DPSetPrimColor, "0", "0", d1, d2, d3, "255"));
                    }
                }
                
                // Call a pipesync if needed
                if (pipesync)
                    list_append(out, generate(DPPipeSync));

                // Update the last texture
                lastMaterial = mat;
            }

            // Load a new vertex block if it hasn't been
            if (!loadedverts)
            {
                char d2[32];
                sprintf(strbuff, "vtx_%s", global_modelname);
                if (ismultimesh)
                {
                    strcat(strbuff, "_");
                    strcat(strbuff, mesh->name);
                }
                strcat(strbuff, "+");
                sprintf(d2, "%d", vertindex);
                strcat(strbuff, d2);
                sprintf(d2, "%d", vcache->verts.size);
                list_append(out, generate(SPVertex, strbuff, d2, "0"));
                vertindex += vcache->verts.size;
                loadedverts = TRUE;
            }
            
            // If we can, dump a 2Tri, otherwise dump a single triangle
            if (!global_no2tri && facenode->next != NULL && ((s64Face*)facenode->next->data)->material == lastMaterial)
            {
                char d1[32], d2[32], d3[32], d4[32], d5[32], d6[32];
                s64Face* prevface = face;
                facenode = facenode->next;
                face = (s64Face*)facenode->data;
                sprintf(d1, "%d", list_index_from_data(&vcache->verts, prevface->verts[0]));
                sprintf(d2, "%d", list_index_from_data(&vcache->verts, prevface->verts[1]));
                sprintf(d3, "%d", list_index_from_data(&vcache->verts, prevface->verts[2]));
                sprintf(d4, "%d", list_index_from_data(&vcache->verts, face->verts[0]));
                sprintf(d5, "%d", list_index_from_data(&vcache->verts, face->verts[1]));
                sprintf(d6, "%d", list_index_from_data(&vcache->verts, face->verts[2]));
                list_append(out, generate(SP2Triangles, d1, d2, d3, "0", d4, d5, d6, "0"));
            }
            else
            {
                char d1[32], d2[32], d3[32];
                sprintf(d1, "%d", list_index_from_data(&vcache->verts, face->verts[0]));
                sprintf(d2, "%d", list_index_from_data(&vcache->verts, face->verts[1]));
                sprintf(d3, "%d", list_index_from_data(&vcache->verts, face->verts[2]));
                list_append(out, generate(SP1Triangle, d1, d2, d3, "0"));
            }

            prevfacenode = facenode;
        }
        
        // Newline if we have another vertex block to load
        if (!isbinary && vcachenode->next != NULL)
            list_append(out, mallocstring("\n"));
    }
    list_append(out, generate(SPEndDisplayList));
    return out;
}


/*==============================
    construct_dltext
    Constructs a display list and stores it
    in a temporary file
==============================*/

void construct_dltext()
{
    FILE* fp;
    linkedList* dl;
    char strbuff[STRBUF_SIZE];
    bool ismultimesh = (list_meshes.size > 1);
    
    // Open a temp file to write our display list to
    sprintf(strbuff, "temp_%s", global_outputname);
    fp = fopen(strbuff, "w+");
    if (fp == NULL)
        terminate("Error: Unable to open temporary file for writing\n");
    
    // Announce we're gonna construct the DL
    if (!global_quiet) printf("Constructing display lists\n");
    
    // Vertex data header
    fprintf(fp, "\n// Custom combine mode to allow mixing primitive and vertex colors\n"
                "#ifndef G_CC_PRIMLITE\n    #define G_CC_PRIMLITE SHADE,0,PRIMITIVE,0,0,0,0,PRIMITIVE\n#endif\n\n\n"
                "/*********************************\n"
                "              Models\n"
                "*********************************/\n\n"
    );
    
    // Iterate through all the meshes
    for (listNode* meshnode = list_meshes.head; meshnode != NULL; meshnode = meshnode->next)
    {
        int vertindex = 0;
        s64Mesh* mesh = (s64Mesh*)meshnode->data;
        
        // Cycle through the vertex cache list and dump the vertices
        fprintf(fp, "static Vtx vtx_%s", global_modelname);
        if (ismultimesh)
            fprintf(fp, "_%s", mesh->name);
        fprintf(fp, "[] = {\n");
        for (listNode* vcachenode = mesh->vertcache.head; vcachenode != NULL; vcachenode = vcachenode->next)
        {
            vertCache* vcache = (vertCache*)vcachenode->data;
            listNode* vertnode;
            
            // Cycle through all the verts
            for (vertnode = vcache->verts.head; vertnode != NULL; vertnode = vertnode->next)
            {
                int texturew = 0, textureh = 0;
                s64Vert* vert = (s64Vert*)vertnode->data;
                n64Material* mat = find_material_fromvert(&vcache->faces, vert);
                Vector3D normorcol = {0, 0, 0};
                
                // Ensure the texture is valid
                if (mat == NULL)
                    terminate("Error: Inconsistent face/vertex texture information\n");
                
                // Retrieve texture/normal/color data for this vertex
                switch (mat->type)
                {
                    case TYPE_TEXTURE:
                        // Get the texture size
                        texturew = (mat->data).image.w;
                        textureh = (mat->data).image.h;
                        
                        // Intentional fallthrough
                    case TYPE_PRIMCOL:
                        // Pick vertex normals or vertex colors, depending on the texture flag
                        if (mat_hasgeoflag(mat, "G_LIGHTING"))
                            normorcol = vector_scale(vert->normal, 127);
                        else
                            normorcol = vector_scale(vert->color, 255);
                        break;
                    case TYPE_OMIT:
                        break;
                }
                
                // Dump the vert data
                fprintf(fp, "    {%d, %d, %d, 0, %d, %d, %d, %d, %d, 255}, /* %d */\n", 
                    (int)round(vert->pos.x), (int)round(vert->pos.y), (int)round(vert->pos.z),
                    float_to_s10p5(vert->UV.x*texturew), float_to_s10p5(vert->UV.y*textureh),
                    (int)round(normorcol.x), (int)round(normorcol.y), (int)round(normorcol.z),
                    vertindex++
                );
            }
        }
        fprintf(fp, "};\n\n");
        
        // Then cycle through the vertex cache list again, but now dump the display list
        fprintf(fp, "static Gfx gfx_%s", global_modelname);
        if (ismultimesh)
            fprintf(fp, "_%s", mesh->name);
        fprintf(fp, "[] = {\n");
        dl = dlist_frommesh(mesh, FALSE);
        for (listNode* dlnode = dl->head; dlnode != NULL; dlnode = dlnode->next)
            fprintf(fp, "%s", (char*)dlnode->data);
        list_destroy_deep(dl);
        fprintf(fp, "};\n\n");
    }
    
    // State we finished
    if (!global_quiet) printf("Finish building display lists\n");
    fclose(fp);
}