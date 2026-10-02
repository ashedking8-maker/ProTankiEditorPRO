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
    std::cout<<"Placement snap: fractional/all-side/rotated edges, separation, floors, XYZ grid and retained height PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
