#include "PlacementSnap.h"
#include <iostream>
#include <stdexcept>
using namespace PlacementSnap;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool near(double a,double b,double e=1e-5){return std::fabs(a-b)<e;}
static Shape box(double x0,double y0,double x1,double y1,double z0=0,double z1=100){return {Hull({{x0,y0},{x1,y0},{x1,y1},{x0,y1}}),z0,z1};}
static Shape rotate(Shape s,double a){for(auto& p:s.hull){const double x=p.x,y=p.y;p={std::cos(a)*x-std::sin(a)*y,std::sin(a)*x+std::cos(a)*y};}s.hull=Hull(s.hull);return s;}
static void separatedAtGuide(const Shape& moving,const Match& m,double gap){
    const auto shifted=Translate(moving,m.dx,m.dy,0);
    const double len=std::hypot(m.guideB.x-m.guideA.x,m.guideB.y-m.guideA.y);
    const Point n{(m.guideB.y-m.guideA.y)/len,-(m.guideB.x-m.guideA.x)/len};
    double lo,hi;Project(shifted,n,lo,hi);
    check(near(lo-Dot(m.guideA,n),gap),"snap must leave exact clearance at supporting edge");
}
int main(){try{
    const auto fixed=box(21.57,-250,421.57,250);
    auto moving=box(400,-100,500,100);
    const auto right=Find(moving,fixed,55);
    check(right.matched&&near(right.dx,21.59)&&near(right.dy,0),"fractional reference edge 421.57");separatedAtGuide(moving,right,.02);
    for(auto s:{box(-100,-100,0,100),box(100,240,200,340),box(100,-340,200,-240)}){
        const auto m=Find(s,fixed,55);check(m.matched,"all four sides");separatedAtGuide(s,m,.02);
    }
    check(!Find(box(5000,0,5100,100),fixed,55).matched,"far objects must stay on regular grid");
    check(!Find(box(400,-100,500,100,500,600),fixed,55).matched,"different floors must not magnet");
    const auto floor=box(0,0,100,100,0,0);
    check(Find(box(99,0,199,100,0,0),floor,10).matched,"flat coplanar tiles can snap");
    check(!Find(moving,fixed,-1).matched&&!Find(moving,fixed,10,-1).matched,"invalid controls rejected");
    check(!Find(Shape{},fixed,55).matched,"missing mesh cannot invent an edge");
    // Arbitrary rotated walls: the chosen support plane separates the footprints.
    for(int angle=0;angle<360;angle+=7){
        const double a=angle*3.141592653589793/180;
        const auto f=rotate(box(-200,-100,200,100),a);
        const auto m=rotate(box(195,-50,295,50),a);
        auto match=Find(m,f,20);check(match.matched,"rotated parallel walls must snap");separatedAtGuide(m,match,.02);
    }
    for(int angle=0;angle<180;angle+=11){
        auto m=rotate(box(-50,-40,50,40),angle*3.141592653589793/180);
        double low,high;Project(m,{1,0},low,high);
        m=Translate(m,421.57-low-3,0,0);
        auto match=Find(m,fixed,10);check(match.matched,"nonparallel corner to side contact");separatedAtGuide(m,match,.02);
    }
    // Render meshes contain duplicate vertices; concave inputs use a conservative outer hull.
    auto h=Hull({{0,0},{1,0},{1,1},{0,1},{0,0},{.5,.5},{1,0}});
    check(h.size()==4,"hull strips duplicates and interior points");
    check(near(Coordinate(421.57f,100,true),400)&&near(Coordinate(421.57f,100,false),421.57,.001),"grid on/off XY");
    check(near(Coordinate(749,100,true,250),750),"clipboard retains source grid phase");
    float height=421.57f;
    check(near(Height(height,0,100,true),height),"asset switch without height input must retain exact Z");
    height=Height(height,1,100,true);check(near(height,500),"Q/E snaps final Z");
    height=Height(height,-1,10,true);check(near(height,490),"Shift fine Z");
    check(near(Height(421.57f,1,100,false),521.57,.001),"disabled snap retains free height phase");
    check(near(Height(250,1,100,true,250),350),"clipboard height phase");
    // Repeat switches/rotations do not accumulate a hidden offset.
    for(int i=0;i<1000;++i)height=Height(height,0,100,true);
    check(near(height,490),"height stable over 1000 asset switches");
    check(near(Tolerance(10,false),5.5)&&near(Tolerance(500,false),10)&&near(Tolerance(500,true),275),"fine vs total tolerance");
    const auto raw=box(390,-50,490,50);
    const auto nearFixed=box(0,-100,424.25,100);
    const auto expected=Find(raw,nearFixed,Tolerance(500,true));
    check(expected.matched&&near(expected.dx,34.27),"capture between coarse grid nodes");
    check(!Find(raw,nearFixed,Tolerance(500,false)).matched,"fine mode must not pull distant object");
    for(int frame=0;frame<1000;++frame){
        Match best;
        // Reference may be any scene object, not the most recently appended one.
        Consider(best,Find(raw,nearFixed,275));
        for(int i=0;i<100;++i)Consider(best,Find(raw,box(10000+i*500,-100,10400+i*500,100),275));
        check(best.matched&&near(best.dx,expected.dx)&&near(best.dy,expected.dy),"stable non-accumulating scene search");
        check(std::hypot(best.dx,best.dy)<=275,"correction bounded by capture radius");
    }
    // Live V5 solver: regular 400/500 remain selectable, with one extra edge node.
    const auto edge444=box(0,-2000,444.57,2000);
    auto discrete=[&](double rawX,double rawY,double step,const Shape& local){
        const double gx=Coordinate(static_cast<float>(rawX),static_cast<float>(step),true);
        const double gy=Coordinate(static_cast<float>(rawY),static_cast<float>(step),true);
        const Point cursor{rawX-gx,rawY-gy};double score=Dot(cursor,cursor);
        auto m=FindDiscrete(Translate(local,gx,gy,0),edge444,1.5*step,.02,cursor,score);
        return Point{gx+(m.matched?m.dx:0),gy+(m.matched?m.dy:0)};
    };
    const auto local=box(0,-20,100,20);
    check(near(discrete(400,0,100,local).x,400),"ordinary node 400 remains available");
    check(near(discrete(444.57,0,100,local).x,444.59),"extra edge node 444.57 plus clearance");
    check(near(discrete(500,0,100,local).x,500),"ordinary node 500 remains available");
    for(int y=-240;y<=240;++y){
        const auto at=discrete(444.57,y,500,local);
        check(near(at.x,444.59)&&near(at.y,0),"500 grid cannot slide continuously along edge");
    }
    check(near(discrete(444.57,300,500,local).y,500),"tangent moves by full 500 step");
    auto grouped=Merge({box(-200,-20,-100,20),box(100,-20,200,20)});
    auto placed=discrete(644.57,0,100,grouped);
    check(near(placed.x,644.59),"copied group outer boundary supplies edge node");
    const double memberA=placed.x-200,memberB=placed.x+100;
    check(near(memberB-memberA,300),"shared group translation preserves spacing");
    const auto diag=rotate(edge444,.43);
    const auto stableShape=rotate(Translate(local,400,0,0),.43);
    Match first;bool captured=false;
    for(int i=0;i<20;++i){
        const Point cursor{40+i*.1,20};double score=Dot(cursor,cursor);
        const auto hit=FindDiscrete(stableShape,diag,150,.02,cursor,score);
        if(hit.matched){if(captured)check(near(hit.dx,first.dx)&&near(hit.dy,first.dy),"diagonal candidate stays discrete inside grid cell");first=hit;captured=true;}
    }
    check(captured,"rotated discrete edge test must exercise a match");
    // Existing-object keyboard motion: stop at the intermediate node, then continue.
    const auto atNextNode=Translate(local,500,0,0);
    double keyboardScore=10000;
    auto firstKey=FindDiscrete(atNextNode,edge444,150,.02,{-100,0},keyboardScore,{100,0});
    check(firstKey.matched&&near(500+firstKey.dx,444.59),"keyboard reaches extra node before 500");
    const double remaining=500-(500+firstKey.dx);
    keyboardScore=remaining*remaining;
    auto secondKey=FindDiscrete(atNextNode,edge444,150,.02,{-remaining,0},keyboardScore,{remaining,0});
    check(!secondKey.matched,"next key leaves current edge rather than sticking");
    keyboardScore=10000;
    auto backwards=FindDiscrete(Translate(local,600,0,0),edge444,300,.02,{-100,0},keyboardScore,{100,0});
    check(!backwards.matched,"keyboard cannot snap backwards");
    std::cout<<"Placement snap: fractional/all-side/rotated edges, separation, floors, XYZ grid and retained height PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
