$fn= $preview ? 8 : 30;

//Tolerance between printed surfaces, accounts for expansion of pla in x/y direction.
tol = 0.1;
pin_tol = 0.2;

Pin_Count=19;
pinx = 2.77;
piny = 2.84;
pinz = -5.1;

insertwall = 1;           //D-Trapezoid wall thickness

insertroundIn  = 2;        //Inner roundness of D-trapezoid shape
insertroundOut = 2.5;     //Outer ...
shroundheight  = 6.10;     //Height of D-Trapezoid shroud


dimA = 19.81 + (10-1)*2.77;   //Total width
dimB = 13.91 + (10-1)*2.77;   //Width to center of drill holes
dimC = 5.84 + (10-1)*2.77;   //Width to inside of shroud
dimD = 12.55;                //Total Height
dimE = 8.38;                 //Height inside shroud

backr=2;
backx= dimA - backr*2;
backy= dimD - backr*2;
backheight = 3.9;

holex=dimB/2;

backingh = 3;

backingz= (backheight + backingh + tol)*-1;

//obsolete height of the backshell
shell_outer_height=20;
shell_inner_height=8.5;
shell_wall=1.5;
//Radius of the wire hole out of the backshell
shell_hole=3;


//Render main parts
plug();

translate([0,-5,0])
    backshell_half();



module backshell_half()
{
    //alignment tab
    color("olive")
    translate([dimA/2/1.5 - .9, -1, -shell_outer_height*.85])
        rotate([0,0,10])
        cube([1,3,2.5],true);
    
    //alignment tab
    color("olive")
    translate([(dimA/2/1.5 - .9)*-1, -1, -shell_outer_height*.65])
        rotate([0,0,-10])
        cube([1,3,2.5],true);
    
    
    difference()
    {
        union()
            {
                translate([0,0,-shell_outer_height-shell_wall])
                    RoundedCube3D(dimA/1.5+shell_wall*2,dimD+shell_wall*2,shell_outer_height+shell_wall*2,2);
                translate([0,0,-shell_inner_height-shell_wall])
                    RoundedCube3D(dimA+shell_wall*2,dimD+shell_wall*2,shell_inner_height+shell_wall*2,2);

                translate([0,0,-shell_outer_height-4])
                    cylinder(r=shell_hole+shell_wall,h=shell_hole+shell_wall);

                translate([0,0,-shell_outer_height-.5])
                    cylinder(r1=shell_hole+shell_wall,r2=shell_hole+shell_wall,h=1);
              
              



            }

       union()
            {

                              //Cut block in half
                color("red")
                    translate([-50, 0,-50])
                    cube([100,20,100]);  

                color("red")
                    translate([-25, -25,0])
                    cube([100,50,50]);  
                
                front_block(0.1);
                //back_block(0.1);
                //Create a simpler block that doesn't have pin cutouts on it
                color("orange")
                translate([0,0,backingz-0.1])
                RoundedCube2D(dimA+0.1*2,dimD+0.1*2,backingh+0.1*2,backr);                

                color("blue")
                translate([0,0,-shell_outer_height])
                    RoundedCube3D(dimA/1.5-1,dimD-1,shell_outer_height,backr);
                color("green")
                translate([0,0,-shell_inner_height])
                    RoundedCube3D(dimA-1,dimD-1,shell_inner_height,backr);

                translate([0,0,-shell_outer_height-10])
                    cylinder(r=shell_hole,h=15);

                //zip tie holes
                color("red")
                    translate([dimA/2/1.5-2.5,-15,-17])
                    cube([1,30,3]);
                color("red")
                    translate([-dimA/2/1.5+1.5,-15,-17])
                    cube([1,30,3]);
            
                logo();

                    
            }
    }


}           

module logo(thickness=1)
{
    translate([0, dimD /-2 - shell_wall + 0.4,shell_outer_height/-2])
        color("black")
        rotate([90,0,0])
        scale([0.70,0.70,1])
         translate([-11.3,-11.3,0])
        linear_extrude(height=thickness)
        import("floppy.svg");
}

module plug()
{
    difference()
    {
        union()
        {
            color("lightblue")
                translate([0,0,0])
                Shroud(0.1);
                
            color("lightgreen")
                front_block();
            
                back_block();
        }
        union()
        {
            color("yellow")
                translate([0,0,-8.19])
                PinArray(pin_tol);

            color("red")
                hole();
        }
    }
    

}

//Back comb part of the plug that holds the back of the pins and wires.
module back_block(tolerance=0)
{
    difference()
    {
                
        //block
        color("orange")
                translate([0,0,backingz-tolerance])
                RoundedCube2D(dimA+tolerance*2,dimD+tolerance*2,backingh+tolerance*2,backr);
               
        //cutout to insert pins
        color("purple")
            translate([0,0,backingz])
            pincoutoutarray(backingh,pin_tol);
        
    }
}

//Front part of the plug that holds the pins, inserted into the back.
module front_block(tolerance=0)
{
    difference()
    {
        color("lightgreen")
            translate([0,0,-backheight-tolerance])
            RoundedCube2D(dimA+tolerance*2,dimD+tolerance*2,backheight+tolerance*2,backr);

        color("magenta")
            translate([0,0,-3])
            front_edge(tolerance);   
    }
}

//Cutout section around the edge of the front block, allows backshell to clamp onto it.
module front_edge(tolerance=0)
{
    difference()
    {
        translate([0,0,tolerance])
        RoundedCube2D(dimA+2,dimD+2,2-tolerance*2,2.5);
        translate([0,0,-0.1])
        RoundedCube2D(dimA-1+tolerance,dimD-1+tolerance,2.2,2);
    }    
    
}


//The D shaped part of the plug that fits over the socket
module Shroud(tolerance=0)
{ 
    difference()
    {
        union()
        {
            color("green")
            trapezium(dimC+insertwall*2, dimE+insertwall*2, shroundheight, insertroundOut,  5);
            color("red")
            trap_fillet(dimC+insertwall*2+2, dimE+insertwall*2+2, 5, insertroundOut+1,  5);
        };
        translate([0,0,-1])
        trapezium(dimC+tolerance*2, dimE+tolerance*2, shroundheight+2, insertroundIn, 5);
    }
}


//Group of cutouts on the back block that holds the pins in place
module pincoutoutarray(dz, tolerance=0)
{
    translate([-4.5*pinx,2.84/2,0])
    for (i=[0:1:9])
    {
     translate([i*pinx,0,0])
        PinCutout(dz,tolerance);
    }   

    translate([-4.5*pinx,2.84/-2,0])
    for (i=[0:1:8])
    {
     translate([(i*pinx)+pinx/2,0,0])
        rotate([0,0,180])
        PinCutout(dz,tolerance);
    }   
}

module PinCutout(dz, tolerance=0)
{
    translate([(1.69+tolerance)/-2,0,0])
    cube([1.69+tolerance,5,dz]);
    
}

//Array of pins
module PinArray(tolerance=0)
{
    translate([-4.5*pinx,2.84/2,0])
    for (i=[0:1:9])
    {
     translate([i*pinx,0,0])
        Pin(tolerance);
    }   

    translate([-4.5*pinx,2.84/-2,0])
    for (i=[0:1:8])
    {
     translate([(i*pinx)+pinx/2,0,0])
        Pin(tolerance);
    }   
}

module Pin(tolerance=0)
{
    
    r1=1.69;
    h1=4.2;
    
    r2=2.15;
    h2a=1.3;
    h2b=0.6;
    
    r3=1.04;
    h3=8.2;
    
    //base
    cylinder(r=(r1+tolerance)/2,h=h1);
    
    //clip
    translate([0,0,h1])
    cylinder(r=(r2+tolerance)/2,h=h2a);

    translate([0,0,h1+h2a])
    cylinder(r1=(r2+tolerance)/2, r2= 1.25 / 2, h=0.6);
    
    //contact area
    translate([0,0,h1+h2a+h2b])
    cylinder(r=(r3+tolerance)/2,h=h3 - (r3/2));
    
    //round end
    translate([0,0,h1+h2a+h2b+h3 - ((r3+tolerance)/2)])
    sphere(r=(r3+tolerance)/2);

}

//Screw holes either side of plug
module hole()
{
    translate([holex,0,-100])
        cylinder(r=3.05/2,h=200);
    translate([-holex,0,-100])
        cylinder(r=3.05/2,h=200);
}
//

//
// Primatives
//

module trapezium(dx, dy, dz, rad, angle)
{
    dh=0.1;
    tx= dx - rad*2;
    ty= dy - rad*2;
    
    tdelta = sin(angle) * dy;
    
    translate([dx/-2,dy/-2,0])
    minkowski()
    {
        translate([rad,rad,0])
        linear_extrude(height=dz-dh)
        polygon(points=[[0,ty],
                        [tx,ty],
                        [tx-tdelta,0],
                        [tdelta,0],
                        [0,ty]]);
        cylinder(r=rad, h=dh);
    }
}

module trap_fillet(dx, dy, dz, rad, angle)
{
    dh=0.1;
    tx= dx - rad*2;
    ty= dy - rad*2;
    
    tdelta = sin(angle) * dy;
    
    translate([dx/-2,dy/-2,0])
    minkowski()
    {
        translate([rad,rad,0])
        linear_extrude(height=0.01)
        polygon(points=[[0,ty],
                        [tx,ty],
                        [tx-tdelta,0],
                        [tdelta,0],
                        [0,ty]]);
        cylinder(r1=rad, r2=0, h=dz);
    }
}



//Generic cube shape rounded on x-y plane.
module RoundedCube2D(dx,dy,dz,rad)
{
    dh=0.1;
    translate([dx/-2,dy/-2,0])
    minkowski()
    {
        translate([rad,rad,0])
        cube([dx - rad * 2, dy - rad * 2, dz - dh ], false);
        cylinder(r=rad, h=dh);
    }

}

//Generic cube shape rounded on all planes
module RoundedCube3D(dx,dy,dz,rad)
{
    
    translate([dx/-2,dy/-2,0])
    minkowski()
    {
        translate([rad,rad,rad])
        cube([dx - rad * 2, dy - rad * 2, dz - rad * 2 ], false);
        sphere(r=rad);
    }
  
}
