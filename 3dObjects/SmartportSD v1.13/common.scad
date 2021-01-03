
//Crude function to add fillet between objects    
module weld(t=1) {
    union() {
        children(0);
        children(1);
        hull(){
            union(){
                intersection() {
                    minkowski(){
                        intersection(){
                            children(0);
                            children(1);
                        }
                        sphere(r=t);
                    }
                    children(0);
                }
                intersection() {
                    minkowski(){
                        intersection() {
                            children(0);
                            children(1);                  
                        }
                        sphere(r=t);
                    }
                    children(1);
                }
            }
        }
    }
}
//

//
// Primatives
//
module RoundedCube2D(dx,dy,dz,rad)
{
    dh=0.1;
    translate([dx/-2,dy/-2,0])
    minkowski()
    {
        translate([rad,rad,0])
        cube([dx - rad * 2, dy - rad * 2, dz - dh ], false);
        translate([0,0,dh/2])
            rotate([0,0,45])
            //cube([rad*2,rad*2,dh],true);
            cylinder(r=rad, h=dh);
    }
}
//
module RoundedCube3D(dx,dy,dz,rad)
{
    translate([dx/-2,dy/-2,0])
    minkowski()
    {
        translate([rad,rad,rad])
            cube([dx - rad * 2, dy - rad * 2, dz - rad * 2 ], false);
        rotate([45,45,45])
            sphere(r=rad);
    }
}
//
module ChiseledCube2D(dx,dy,dz,rad)
{
    dh=0.1;
    translate([dx/-2,dy/-2,0])
    minkowski()
    {
        translate([rad,rad,0])
        cube([dx - rad * 2, dy - rad * 2, dz - dh ], false);
        translate([0,0,dh/2])
            rotate([0,0,45])
            cube([rad*2,rad*2,dh],true);
    }
}
//

module ChiseledCube3D(dx,dy,dz,rad)
{
    translate([dx/-2,dy/-2,0])
    minkowski()
    {
        translate([rad,rad,rad])
            cube([dx - rad * 2, dy - rad * 2, dz - rad * 2 ], false);
        rotate([45,45,45])
            cube([rad*2,rad*2,rad*2],true);
    }
}
//