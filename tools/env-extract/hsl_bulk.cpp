// hsl_bulk — exhaustive ("全量") official-env asset dump.
//
// Unlike hsl_extract (which is scene-graph faithful and only exports what libshell
// DRAWS from firstWorldAssetId — e.g. Vista's 29 MG-shell drawables), this tool walks
// the ENTIRE cooked asset store inside assets/scene.zip and decodes EVERY RENDMESH and
// EVERY RENDTXTR it finds, regardless of scene reachability. That recovers the sublevels
// the runtime streams separately (fgpockets: boulders/plants/logs; cagemeshes: distant
// treeline/valley cards) plus every texture, so nothing in the pack is left behind.
//
// Output: <name>.gltf + <name>.bin (all meshes) and textures/ (every decoded texture,
// named by its source path). Meshes are placed at their own local centroid (the cooked
// sub-target geometry is level-local); precise world placement is layered on separately
// via the USD/HSTF transforms in the scene-graph pass. Base-color textures are bound to
// meshes by name heuristic; ALL textures are also dumped standalone so none are lost.
//
// Meta-proprietary assets — personal-device research only; never redistribute.

#include "loaders/rendmesh_parser.h"
#include "loaders/rendtxtr_parser.h"
#include "io/gltf_export.h"
#include "miniz.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>

typedef unsigned long long u64x;

static std::string lower(std::string s){ for(auto&c:s)c=(char)tolower((unsigned char)c); return s; }

// last non-empty path segment
static std::string leaf(const std::string& p){
    size_t e=p.size(); while(e>0 && p[e-1]=='/') --e;
    size_t s=p.rfind('/', e? e-1 : 0); s=(s==std::string::npos)?0:s+1;
    return p.substr(s, e-s);
}

// core token of a mesh leaf: strip common cook prefixes / trailing 4-char hash, lowercase.
static std::string meshToken(std::string n){
    n=lower(n);
    // drop trailing "_xxxx" cook hash
    size_t u=n.rfind('_'); if(u!=std::string::npos && n.size()-u-1>=3 && n.size()-u-1<=5) n=n.substr(0,u);
    const char* pres[]={"rootnode_","root_xform_","root_","sm_mg_","sm_lbg_","sm_","m_"};
    for(auto pre:pres){ if(n.rfind(pre,0)==0){ n=n.substr(strlen(pre)); break; } }
    return n;
}
// source filename of a texture entry: the ".../NAME.png/<hash>/tex_r676" segment.
static std::string texSourceName(const std::string& p){
    size_t png=p.find(".png/"); size_t end=(png==std::string::npos)?p.rfind('/'):png;
    if(end==std::string::npos) return leaf(p);
    size_t s=p.rfind('/', end? end-1:0); s=(s==std::string::npos)?0:s+1;
    return p.substr(s, end-s);
}

int main(int argc, char** argv){
    if(argc<3){ fprintf(stderr,"usage: hsl_bulk <env.apk|scene.zip> <outDir> [name]\n"); return 2; }
    std::string in=argv[1], outDir=argv[2], name=(argc>3&&argv[3][0])?argv[3]:"env_full";
    bool verbose = std::getenv("HSL_VERBOSE")!=nullptr;

    // Load the container; if it's an APK, lift assets/scene.zip into memory first.
    std::vector<u8> zipBytes;
    { FILE* f=fopen(in.c_str(),"rb"); if(!f){ fprintf(stderr,"cannot open %s\n",in.c_str()); return 1; }
      fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
      zipBytes.resize(n); if(fread(zipBytes.data(),1,n,f)!=(size_t)n){ fclose(f); return 1; } fclose(f); }

    mz_zip_archive z{}; 
    std::vector<u8> sceneBuf;
    if(!mz_zip_reader_init_mem(&z, zipBytes.data(), zipBytes.size(), 0)){ fprintf(stderr,"zip init failed\n"); return 1; }
    // APK? extract assets/scene.zip and reopen.
    int sidx=mz_zip_reader_locate_file(&z,"assets/scene.zip",nullptr,0);
    if(sidx>=0){
        size_t sz=0; void* p=mz_zip_reader_extract_to_heap(&z,sidx,&sz,0);
        if(!p){ fprintf(stderr,"extract scene.zip failed\n"); return 1; }
        sceneBuf.assign((u8*)p,(u8*)p+sz); mz_free(p);
        mz_zip_reader_end(&z); z=mz_zip_archive{};
        if(!mz_zip_reader_init_mem(&z, sceneBuf.data(), sceneBuf.size(), 0)){ fprintf(stderr,"scene.zip init failed\n"); return 1; }
    }

    mz_uint total=mz_zip_reader_get_num_files(&z);
    std::vector<MeshData> meshes;
    // decoded textures: sourceName -> (rgba,w,h)
    struct Tex{ std::vector<u8> rgba; u32 w,h; };
    std::map<std::string,Tex> texByName;
    int nMesh=0,nMeshFail=0,nTex=0,nTexFail=0;

    for(mz_uint i=0;i<total;i++){
        mz_zip_archive_file_stat st; if(!mz_zip_reader_file_stat(&z,i,&st)) continue;
        std::string path=st.m_filename;
        if(path.empty()||path.back()=='/') continue;
        bool isMesh = path.find("__mesh_sub_targets___ms61/")!=std::string::npos
                   && path.find("__material_sub_targets")==std::string::npos;
        bool isTex  = leaf(path).rfind("tex_r676",0)==0;
        if(!isMesh && !isTex) continue;

        size_t sz=0; void* pd=mz_zip_reader_extract_to_heap(&z,i,&sz,0); if(!pd) continue;
        std::vector<u8> data((u8*)pd,(u8*)pd+sz); mz_free(pd);

        if(isMesh){
            if(data.size()<8||memcmp(data.data()+4,"MESH",4)!=0){ continue; }
            MeshData md; md.name=leaf(path); md.meshPath=path;
            if(parseRendMesh(data,md.positions,md.uvs,md.indices,&md.boneIndices,&md.boneWeights,
                             md.name.c_str(),&md.uvs2,&md.bonePalette,&md.colors,&md.uvs3,&md.uvs4)
               && md.positions.size()>=9 && md.indices.size()>=3){
                meshes.push_back(std::move(md)); nMesh++;
            } else { nMeshFail++; if(verbose) fprintf(stderr,"[mesh-skip] %s\n",path.c_str()); }
        } else {
            RendtxtrInfo ti;
            if(data.size()<8||memcmp(data.data()+4,"TXTR",4)!=0){ nTexFail++; continue; }
            if(!parseRendtxtrHeader(data,ti)){ nTexFail++; if(verbose)fprintf(stderr,"[tex-hdr] %s\n",path.c_str()); continue; }
            std::vector<u8> rgba;
            bool hdr = path.find(".hdr")!=std::string::npos || path.find("lmhdr")!=std::string::npos;
            u32 payLen = ti.rawDataLen / (ti.faces?ti.faces:1);   // cubemap: decode face 0
            if(!astc::decodeASTC(data.data()+ti.rawDataOffset, payLen, ti.width, ti.height,
                                 ti.blockW, ti.blockH, rgba, hdr) || rgba.size()<(size_t)ti.width*ti.height*4){
                nTexFail++; if(verbose)fprintf(stderr,"[tex-dec] %s\n",path.c_str()); continue;
            }
            texByName[lower(texSourceName(path))]=Tex{std::move(rgba),ti.width,ti.height}; nTex++;
        }
    }
    mz_zip_reader_end(&z);

    // ── name-based base-color binding: mesh core token vs texture source name ──
    int bound=0;
    for(auto& md:meshes){
        std::string tok=meshToken(md.name);
        if(tok.size()<3) continue;
        const Tex* best=nullptr; std::string bestk;
        for(auto& kv:texByName){
            const std::string& tn=kv.first;
            bool base = tn.find("_bc")!=std::string::npos||tn.find("basecolor")!=std::string::npos
                      ||tn.find("_d.")!=std::string::npos||tn.rfind("_d",tn.size()-2)!=std::string::npos
                      ||tn.find("_albedo")!=std::string::npos||tn.find("diffuse")!=std::string::npos;
            if(tn.find(tok)!=std::string::npos && (base||best==nullptr)){
                if(base){ best=&kv.second; bestk=tn; break; }
                if(!best){ best=&kv.second; bestk=tn; }
            }
        }
        if(best){ md.texRGBA=best->rgba; md.texW=best->w; md.texH=best->h; md.hasTexture=true; bound++; }
    }

    // export combined glTF (all meshes)
    bool ok=gltfexport::exportEnv(meshes,outDir,name,"");

    // dump EVERY decoded texture standalone (nothing lost even if unbound)
    namespace fs=std::filesystem; std::error_code ec;
    fs::create_directories(fs::path(outDir)/"textures_all",ec);
    int dumped=0;
    for(auto& kv:texByName){
        std::string fn=kv.first; for(auto&c:fn) if(c=='/'||c=='\\')c='_';
        if(gltfexport::writePng((fs::path(outDir)/"textures_all"/(fn+".png")).string(),
                                kv.second.rgba.data(),kv.second.w,kv.second.h)) dumped++;
    }

    fprintf(stderr,"[bulk] meshes: %d ok / %d fail | textures: %d ok / %d fail | bound base-color: %d | dumped: %d\n",
            nMesh,nMeshFail,nTex,nTexFail,bound,dumped);
    fprintf(stderr,"[bulk] %s -> %s/%s.gltf (%zu meshes)\n", ok?"OK":"FAILED", outDir.c_str(), name.c_str(), meshes.size());
    return ok?0:1;
}
