include <core.scad>
include <vitamins/pcb.scad>
include <../../common.scad>

arrange= $preview ? false : true;


//Wall thickness
wall=1.5;

//Gap size, to allow for nice fittment of 3D printed parts
tol=0.1;

//Overall dimensions of the box
box_x=100;
box_y=60;
box_z=30;
box_r=1;
box_cut= 22;


stripe_gap= box_y / 12; //5;

//Top groove dimensions
groove_x=75;
groove_xoff=-30;
groove_y=1;
groove_depth=0.5;

//Details of the front slot
slot_ele=16.5;
slot_w=box_y*.75;
slot_h=2;

//
// Spawn sub-assemblies
//


/*
if (arrange)
    translate([0,box_y+5,30.7])
        color("red")
        rotate([180,0,0])
        case_top();
else
    translate([0,0,10])
        case_top();
*/

if (arrange)
    translate([0,0,0.7])
        color("green")
        case_bottom();
else
    color("silver",.1)
    case_bottom();

if (arrange)
    translate([0,-box_y,28.4])
        color("yellow")
        rotate([-90,0,0])
        activity_light_tube();
else
    activity_light_tube();

arduino_uno();
storage_shield();

translate([box_x/2-11.8,0,box_cut-8.5])
rotate([0,180,-90])
usbBoard();

translate([box_x/2-4.5,-10,6.8])
color("yellow")
cube([7,7,13.7],true);

translate([box_x/2-4.5,10,6.8])
color("yellow")
cube([7,7,13.7],true);

translate([box_x/2-13.0,0,1+2.5])
color("yellow")
cube([26,27,5],true);

//
// Modules
//

module usbBoard()
{
    translate([0,0,.6])
    color("darkgreen")
    cube([23,17,1.6],true);
    
    translate([0,-1.0,-.5])
    color("silver")
    cube([23,9,1],true);
    
    translate([0,4,10.86/2+1.6])
    color("silver")
    cube([12.12,17,10.86],true);
}

module case_top()
{
    union()
    {


                
        difference()
        {
            union()
            {
                //Alignment piece
                translate([0,0,box_cut-2])
                    color("lightblue")
                    innerband(box_x-wall*2-tol*2,box_y-wall*2-tol*2,box_z-box_cut+2,1);

                //Piece that connects alignment piece to lid
                translate([0,0,box_cut])
                    color("lightgreen")
                    innerband(box_x-wall,box_y-wall,box_z-box_cut,1);

                //Flat piece above door 
                translate([box_x/-2-.5,(slot_w-tol)/-2,slot_ele+slot_h])
                    color("olive")
                    cube([wall+tol*2,slot_w-tol,6+tol]);  
            }
            union()
            {
                //Activity LED cutout
                translate([box_x/-2 - 1 - wall,box_y/2 - 6-tol/2 - wall,box_cut-2])
                    color("seagreen")
                    cube([8+tol,6.5,2]); 
                
            }
        }
        
        difference()
        {
            union()
            {
                case(); 
            }

            
            union()
            {
                //Cut off the bottom of the case
                translate([-box_x,-box_y,-5])
                    color("red")
                    cube([box_x*2,box_y*2,5+box_cut-tol]);
                
                //Cutout around the outside of the bottom of the lid
                translate([0,0,box_cut-tol*2])
                    color("orange")
                    innerband(box_x+wall,box_y+wall,1,1);
                
                //Grooves on top
                for (a=[-5:4])
                    color("purple")
                        translate([groove_xoff,stripe_gap*a+0.5*stripe_gap,box_z-groove_depth]) 
                        cube([groove_x,groove_y,10]);

                //Hole to access reset button
                translate([-40.8,23.2,0])
                    color("yellow")
                    cylinder(r=1,h=50);


            }
        }
    }
}
//

            

module activity_light_tube()
{
translate([box_x/-2-.5,(slot_w)/2+tol,box_cut-2+tol])
    color("lightblue",0.3)
        union()
        {
            //Top
            cube([6,6-tol*2,2-tol*2]);
            
            translate([10,4.2,-4])
                cube([70,1.6,2-tol*2]);
            
            translate([2,4.2,0])
            rotate([0,25,0])
            cube([9,1.6,2-tol*2]);

            difference()
            {
                translate([79,5.20,-3.15])
                    rotate([0,100,-10])
                    difference()
                    {
                        cylinder(r1=1,r2=2.5,h=10);
                        //3mm LED cutout
                        rotate([0,180,0])
                        translate([0,0,-11])
                        union()
                        {
                            cylinder(r=3.74/2+tol,h=1+tol);
                            translate([0,0,1])  
                                cylinder(r=3/2+tol,h=4.49-1.5);
                            translate([0,0,1+4.49-1.5])
                                sphere(r=1.5+tol);
                        }
                    }
                //chop side off so it lays flat when printing
                translate([75,5.8,-10])
                cube([20,10,10]);
            }
            
  
        }
}
//

module case_bottom()
{
    difference()
    {
        union()
        {
            case();
            
            //Activity LED shelf
            translate([box_x/-2 - 1 + wall,box_y/2 - 6 - wall,box_cut-4])
                color("yellow")
                cube([5,4.2,2]);
            
            color("olive")
                translate([-47.3, -26.68, wall-1])
                union()
                {
                translate([15.24, 51.5])    //old values [15.24, 50.8]
                    post();
                translate([66.04, 35.56])
                    post();
                translate([66.04, 7.62])
                    post();
                translate([13.97, 2.54])
                    post();
                }


              //shelf to hold up light tube near back
                translate([box_x/-2-.5,(slot_w)/2+tol,box_cut-2+tol])
                union()
                {
                    translate([70,2,-5-tol])
                        cube([8,5,1]);
                    translate([70,2,-5-tol])
                        cube([8,2,2]);
                }

        }
        union()
        {
            //Cut off top of the case
            translate([-box_x,-box_y,box_cut])
                color("red")
                cube([box_x*2,box_y*2,5+box_cut]);
            
            //Disk slot cutout
            translate([box_x/-2-wall*.5,(slot_w+tol)/-2,slot_ele])
                color("lime")
                cube([wall*2,slot_w+tol,6+tol]);
            
            //Activity LED cutout
            translate([box_x/-2 - 1 - wall,box_y/2 - 6 - wall,box_cut-2])
                color("pink")
                cube([5,6,2]);
            
            //Vent slots in back
            for (a=[-10:6])
                translate([box_x/2-2.5,a*2.3,13])
                    color("yellow")
                    cube([5,1,6]);

            //Wire hole in the back
            translate([box_x/2-5,box_y/2-8,16])
                color("red")
                rotate([0,90,0])
                cylinder(r=2.5,h=10);
            translate([box_x/2-5,box_y/2-8-2.5,16])
                color("red")
                cube([10,5,10]);
            
            //Apple Logo
/*
            translate([box_x/-2,box_y/2*.75,4])
                color("lime")
                rotate([90,0,-90])
                scale([0.015,0.015,0.1])
                linear_extrude(50)
                import("logo.svg");
*/
        }
    }
}
//


module case(extra=0)
{
    difference()
    {
        ChiseledCube3D(box_x,box_y,box_z,box_r);

        //Hollow it out
        translate([0,0,(wall+extra)])
            ChiseledCube3D(box_x-(wall+extra)*2 ,box_y-(wall+extra)*2,box_z-(wall+extra)*2,box_r);
    }  
}
//

module innerband(ix=0,iy=0,iz=0,ir=0,iw=wall)
{
    difference()
    {
        ChiseledCube2D(ix,iy,iz,ir);
        translate([0,0,-iw])
            ChiseledCube2D(ix-iw*2,iy-iw*2,iz+iw*2,ir);       
    }
    
}
//


//Posts for the Arduino to sit on.
module post()
{
    cylinder(r1=3.5,r2=3,h=2);
    translate([0,0,2])
        cylinder(r2=3.0/2, r1=3.3/2, h=2);
}
//

module arduino_uno()
{
    translate([-13,0,-2.4])
    pcb_assembly(["ArduinoUno3", "Arduino Uno R3", 68.58, 53.34, 1.6, 0, 3.3, 0, "#2140BE", false, [],
        [[30.226, -2.54, 0, "2p54socket", 10, 1],
         [54.61,  -2.54, 0, "2p54socket", 8, 1],
         [36.83,   2.54, 0, "2p54socket", 8, 1],
         [57.15,   2.54, 0, "2p54socket", 6, 1],
         [64.91,  27.89, 0, "2p54header", 2, 3],
         [18.796, -7.00, 0, "2p54header", 3, 2],
         [ 6.5,   -3.5,  0, "button_6mm"],
         //[4.7625,  7.62, 180, "barrel_jack"],
         //[1.5875, 37.7825,180,"usb_B"],
        ],
        [],[],
        inch([
         [-1.35, -1.05],
         [-1.35,  1.05],
         [ 1.19,  1.05],
         [ 1.25,  0.99],
         [ 1.25,  0.54],
         [ 1.35,  0.44],
         [ 1.35, -0.85],
         [ 1.25, -0.95],
         [ 1.25, -1.05],
        ]),
        M2p5_pan_screw
       ],5,3);

}
//

module storage_shield()
{
translate([-13,0,12.5-2.4])
pcb_assembly(["ArduinoUno3", "Arduino Uno R3", 68.58, 53.34, 1.6, 0, 3.3, 0, "#2140BE", false, [],
    [
     [ 6.5,   -3.5,  0, "button_6mm"],
     [12.75,30,0,"chip",25.55,26.5,1.5],
    ],
    [],[],
    inch([
     [-1.35, -1.05],
     [-1.35,  1.05],
     [ 1.19,  1.05],
     [ 1.25,  0.99],
     [ 1.25,  0.54],
     [ 1.35,  0.44],
     [ 1.35, -0.85],
     [ 1.25, -0.95],
     [ 1.25, -1.05],
    ]),
    M2p5_pan_screw
   ],5,3);

}
//
