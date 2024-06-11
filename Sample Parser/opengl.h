#ifndef _SAUSN64_OPENGL_H
#define _SAUSN64_OPENGL_H

    typedef struct {
        uint16_t vertcount;
        uint16_t vertoffset;
        uint16_t facecount;
        uint16_t faceoffset;
        n64Texture* tex;
        int32_t matid;
    } VCacheRenderBlock;

    extern linkedList* generate_opengl_vcachelist();
    extern void construct_opengl();
    
#endif