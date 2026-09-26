#include "Native3DSWriter.h"
#include "NativeCollisionImport.h"
#include "ReleaseTestCheck.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static std::uint16_t U16(const Native3DSWriter::Bytes& b,size_t p){return std::uint16_t(b[p])|(std::uint16_t(b[p+1])<<8);}
static std::uint32_t U32(const Native3DSWriter::Bytes& b,size_t p){return std::uint32_t(U16(b,p))|(std::uint32_t(U16(b,p+2))<<16);}
int main(){
    Native3DSWriter::Model m;
    m.visualName="ptpro_mesh";
    m.vertices={{-250.f,0.f,-250.f,0.f,0.f},{250.f,0.f,-250.f,1.f,0.f},
                {-250.f,200.f,250.f,0.f,1.f},{250.f,200.f,250.f,1.f,1.f}};
    m.indices={0,1,2,1,3,2};
    m.parts={{0,6,"ptpro_mat_0","land21.jpg"}};
    m.boxes={{{-100.f,-90.f,0.f},{100.f,90.f,110.f}},
             {{120.f,-90.f,0.f},{220.f,90.f,110.f}}};
    Native3DSWriter::Bytes bytes;std::string error;
    PT_REQUIRE(Native3DSWriter::Write(m,bytes,error));
    PT_REQUIRE(U16(bytes,0)==0x4d4d && U32(bytes,2)==bytes.size());
    size_t objects=0,helpers=0,materials=0,keyNodes=0;
    bool valid=true;bool sawReversed=false;
    auto walk=[&](auto&& self,size_t a,size_t end,int depth)->void{
        if(depth>20){valid=false;return;}
        while(a<end&&valid){
            if(a+6>end){valid=false;break;}
            const auto id=U16(bytes,a);
            const auto len=U32(bytes,a+2);
            if(len<6||a+len>end){valid=false;break;}
            auto p=a+6;const auto last=a+len;
            if(id==0x4000){
                auto z=std::find(bytes.begin()+static_cast<std::ptrdiff_t>(p),
                    bytes.begin()+static_cast<std::ptrdiff_t>(last),0);
                if(z==bytes.begin()+static_cast<std::ptrdiff_t>(last)){valid=false;break;}
                const std::string name(bytes.begin()+static_cast<std::ptrdiff_t>(p),z);
                ++objects;if(name.rfind("Box",0)==0)++helpers;
                p=static_cast<size_t>(z-bytes.begin())+1;
            }
            if(id==0xAFFF)++materials;
            if(id==0xB002)++keyNodes;
            if(id==0x4120 && last-p>=2+2*8 && U16(bytes,p)==2 &&
                U16(bytes,p+2)==0 && U16(bytes,p+4)==2 && U16(bytes,p+6)==1)
                sawReversed=true;
            if(id==0x4d4d||id==0x3d3d||id==0x4000||id==0x4100||id==0xb000||id==0xb002)
                self(self,p,last,depth+1);
            a=last;
        }
    };
    walk(walk,0,bytes.size(),0);
    PT_REQUIRE(valid && objects==3 && helpers==2 && materials==1 && keyNodes==3 && sawReversed);
    const auto scratch=std::filesystem::temp_directory_path()/
        ("ptpro_0524_writer_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".3ds");
    {
        std::ofstream out(scratch,std::ios::binary|std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        PT_REQUIRE(static_cast<bool>(out));
    }
    const auto native=NativeCollisionImport::Read(scratch);
    std::error_code ec;std::filesystem::remove(scratch,ec);
    PT_REQUIRE(native.Valid() && native.visualAnchor=="ptpro_mesh" && native.boxes.size()==2 &&
        native.planes.empty() && native.triangles.empty());
    PT_REQUIRE(std::abs(native.boxes[0].offset.z-55.f)<.02f &&
        std::abs(native.boxes[0].size.x-200.f)<.02f &&
        std::abs(native.boxes[1].offset.x-170.f)<.02f);
    auto bad=m;bad.indices[2]=999;
    PT_REQUIRE(!Native3DSWriter::Write(bad,bytes,error));
    bad=m;bad.boxes[0].max[0]=bad.boxes[0].min[0];
    PT_REQUIRE(!Native3DSWriter::Write(bad,bytes,error));
    bad=m;bad.parts[0].texture="../unsafe.jpg";
    PT_REQUIRE(!Native3DSWriter::Write(bad,bytes,error));
    std::cout<<"Native3DSWriter: visual mesh, winding, two native-read boxes, materials, keyframes, invalid inputs PASS\n";
}
