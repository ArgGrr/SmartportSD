gap=0.1;
wall=1.5;

header_x=2.54;
header_y=2.54;
header_z=2.54;

//TFT specifics
qd_pcb_X=53.47;
qd_pcb_Y=44.96;
qd_pcb_Z=1.5;
qd_pcb_lift= 8.6+gap+header_z;   //11.24

qd_lcd_Z=3;

//Arduino board dimensions
ard_X=33;
ard_Y=18;
ard_Z=1.5;


//Base PCB dimensions
base_pcb_X=53.47;   //55
base_pcb_Y=55.88;   //57
base_pcb_Z=1.5;

pcb_padding=1;

//Case specifics
case_lwr_void=5;
case_lwr_split=7.9;
case_rounding=0.5;


$fn= $preview ? 16 : 32;

//Location of port cutouts
usb_cutout=[55,21.75+2.54*5,6.5-2.54/2];
sd_cutout=[27.5,0,9.5];
ribbon_cutout=[26.5,45+2.54*5,6.5+0.8];

//2.66 to tall, before lcd pcb lift adjust
//real lcd pcb lifft: 11.30
 
                
if ($preview)
{
    //color("cyan",0.8)    %case_lower();
    translate([0,0,-0]) case_lower();
    
    //color("cyan",0.8)    %case_upper();
    translate([0,0,0]) case_upper();
    
    translate([0,2.54*4,0]) spacer();

    internals();
    
}
else
{
    //3d print layout
    translate([0,0,case_lwr_void+3])    case_lower();
    translate([65,55,17.2375])    rotate([180,0,0])   case_upper();
    translate([0,65,-gap])    spacer();
    translate([65,80,17.25])    rotate([180,0,0])   button_cap();
}




//top
module case_upper()
{

    
    difference()
    {
        color("SlateBlue")
        union()
        {
            translate([-wall-pcb_padding,-wall-pcb_padding,case_lwr_split])
            roundcube(base_pcb_X+wall*2+pcb_padding*2, base_pcb_Y+wall*2+pcb_padding*2, qd_pcb_lift + qd_lcd_Z - case_lwr_split + 1.5 + wall, case_rounding, false);
                
         }
        union()
        {
                        
            //section
//            if ($preview)
//                color("red")
//                    translate([30,30,-25])
//                    cube([60,60,50]);
            
            //USB plug hole
            translate(usb_cutout)
                color("purple")
                roundcube(10,12,8,1,true);
            //SD card hole
            translate(sd_cutout)
                color("purple")
                roundcube(28,10,5,1,true);     

            //Upper void
            color("lime")
                translate([-gap-pcb_padding, -gap-pcb_padding,case_lwr_split-gap])
                cube([base_pcb_X+gap*2+pcb_padding*2, base_pcb_Y+gap*2+pcb_padding*2, case_lwr_split]);     
   
            //lcd cutout
            grid(0,4)
                translate([3.6,6.2,qd_pcb_lift + qd_lcd_Z - case_lwr_split + 9])
                roundcube(38,32,20,1,false);        
            
            //led cutout
            grid(2,2)
                translate([2.54/-2,2.54/-2,qd_pcb_lift + qd_lcd_Z - case_lwr_split + 9 - gap])
                cube([5.4,2.4,10+wall]);
            
            //Button cutout
            grid(15,1)
                translate([2.2,-0.6,qd_pcb_lift + qd_lcd_Z - case_lwr_split + 9 - gap])
                roundcube(8.35,6.3,10+wall,0.6,false);
            grid(15,1)
                translate([2.2-1,-0.6-1,qd_pcb_lift + qd_lcd_Z - case_lwr_split + 9 - gap+wall/2])
                roundcube(10.35,8.3,10+wall,1,false);
                
            //"Consolas:style=Bold"
            //Text
            translate([-0.1+24,-0.3+13.5,qd_pcb_lift + qd_lcd_Z - case_lwr_split + 10 - gap + 0.5])
                linear_extrude(20)
                text("Smartport SD",font="Consolas:style=Bold",size=5,halign="center",valign="center");
        }
    }

    
    //Lugs
    color("blue")
        translate([0.4-pcb_padding,0.7,5])
        cube([1.5,9.1,qd_pcb_lift + qd_lcd_Z - case_lwr_split+5]);

    color("green")
        translate([-1.2-pcb_padding,0.7,8])
        cube([2,9.1,qd_pcb_lift + qd_lcd_Z - case_lwr_split+2]);

    color("red")
        translate([1-pcb_padding,9.8,5])
        rotate([90,0,0])
        cylinder(r=1.5,h=9.1);

    color("blue")
        translate([base_pcb_X-1.2-1.0+pcb_padding,0.7,5])
        cube([1.5,9.1,qd_pcb_lift + qd_lcd_Z - case_lwr_split+5]);

    color("green")
        translate([base_pcb_X-1.2-0.5+pcb_padding,0.7,8])
        cube([2,9.1,qd_pcb_lift + qd_lcd_Z - case_lwr_split+2]);

    color("red")
        translate([base_pcb_X-1.2+pcb_padding,9.8,5])
        rotate([90,0,0])
        cylinder(r=1.5,h=9.1);
    
    
}//
//bottom
module case_lower()
{
    difference()
    {
        union()
        {
            translate([-wall-pcb_padding,-wall-pcb_padding,-wall-case_lwr_void-base_pcb_Z])
            roundcube(base_pcb_X+wall*2+pcb_padding*2,base_pcb_Y+wall*2+pcb_padding*2, case_lwr_split + case_lwr_void + wall + base_pcb_Z,case_rounding,false);
            
           
        }
        union()
        {
            //pcb shelf void
            color("lime")
            translate([-gap-pcb_padding,-gap-pcb_padding,-base_pcb_Z-gap])
            cube([base_pcb_X+gap*2+pcb_padding*2, base_pcb_Y+gap*2+pcb_padding*2, 10]);
            
            //void
            color("cyan")
            translate([-gap-pcb_padding,2.54-pcb_padding,-case_lwr_void-base_pcb_Z])
            cube([base_pcb_X+gap*2+pcb_padding*2, base_pcb_Y-2.54*2+pcb_padding*2, 10]);
            
            color("cyan")
            translate([2.54-pcb_padding,-gap-pcb_padding,-case_lwr_void-base_pcb_Z])
            cube([base_pcb_X-2.54*2+pcb_padding*2, base_pcb_Y+gap*2+pcb_padding*2, 10]);
            
            //section
            if ($preview)
                color("red")
                    translate([30,30,-25])
                    cube([60,60,50]);

            
            //USB plug hole
            translate(usb_cutout)
                color("purple")
                roundcube(10,12,8,1,true);
            //SD card hole
            translate(sd_cutout)
                color("purple")
                roundcube(28,10,5,1,true);
                
            //Drive Ribbon
            translate(ribbon_cutout)
                color("purple")
                translate([0,0,-5.5])
                roundcube(50,10,4,0.4,true);      
            
            //Lug cutouts
            color("red")
                    translate([1-pcb_padding,10,5])
                        rotate([90,0,0])
                        cylinder(r=1.7,h=9.5);
            color("red")
                    translate([base_pcb_X-1+pcb_padding,10,5])
                        rotate([90,0,0])
                        cylinder(r=1.7,h=9.5);
                        
            translate([base_pcb_X/2,base_pcb_Y+pcb_padding+wall-0.5,-3])
                rotate([90,0,180])
                linear_extrude(200)
                text("1 ------------- 19",font="Consolas:style=Bold",size=4,halign="center",valign="center");
                        
        }
    }

}//

//spacer
module spacer()
{
    color("red")
    translate([0,0,gap])
    difference()
    {
        union()
        {
            translate([3,gap,0])     cube([10,5,qd_pcb_lift-gap*2]);
            translate([3+10,gap,0])  cube([30,5,qd_pcb_lift-5]);
            translate([3+40,gap,0])  cube([10,5,qd_pcb_lift-gap*2]);

            translate([3,35,0])      cube([50,5,qd_pcb_lift-gap*2]);

            translate([3,gap,0])     cube([2.54,40,qd_pcb_lift-gap*2]);
            
            translate([5,2.4,qd_pcb_lift-0.5])     cylinder(r=1.2,h=qd_pcb_Z+0.5);
            translate([5+45,2.4,qd_pcb_lift-0.5])  cylinder(r=1.2,h=qd_pcb_Z+0.5);
        }
        union()
        {
            translate([38,0,0])      cube([20,3,5]);
            
                color("purple")
    translate([3+5,34,qd_pcb_lift-gap*2-5])
    cube([15,7,5.1]);
        }
    }
    

    


}//



module grid(x=0,y=0,z=0)
{
    translate([2.54/2+2.54*x,2.54/2+2.54*y,2.54*z])
    children();
}

//PCB and internal components
module internals()
{
    
    difference()
    {
        union()
        {
            //Base PCB
            color("SaddleBrown")
            translate([-pcb_padding,-pcb_padding,-base_pcb_Z])
            pcb(base_pcb_X+pcb_padding*2,base_pcb_Y+pcb_padding*2,base_pcb_Z);

            //TFT header/ribbon
            grid(0,5)  pinsocket(1,16,1);
            //Smart Port Ribbon
            grid(1,21) pinheader(19,1,1);

            //Screen
            grid(0,4) tft_sd();

            //Micro
            grid(8,10,1)  ArduinoMicro();
            grid(8,10)    pinheader(12,1,1);
            grid(8,16)    pinheader(12,1,1);

            //resistors
            grid(2,10) resistor(90,"red","purple","red");
            grid(2,11) resistor(90,"red","purple","red");
            grid(2,12) resistor(90,"red","purple","red");
            grid(2,14) resistor(90,"red","purple","red");
            grid(2,15) resistor(90,"red","purple","red");

            grid(4,2)  resistor(90,"brown","black","brown");

            grid(2,2)  translate([0,0,8.50 + wall])   led_flat();
            grid(15,1) push_button();            
        }
        
        //trim leads
        color("red")
            translate([0,0,-25-3])
            cube([200,200,50],true);
    }
    

  
} 

/******************************************************
*******************************************************
**                                                   **
** Components                                        **
**                                                   **
*******************************************************
*******************************************************/

module ArduinoMicro(rotation=0)
{
    //Arduino Micro
    rotate([0,0,-rotation])
    translate([2.54/-2,2.54/-2,0])
    union()
    {
        color("darkblue")    
            cube([ard_X,ard_Y,ard_Z]);

        translate([29,(ard_Y-7.6)/2,ard_Z])
            color("grey")
            cube([5.7,7.6,2]);
        
        color("black")
        translate([15,ard_Y/2,ard_Z+0.5])
        rotate([0,0,45])
        cube([7,7,1],true);
        
        color("silver")
        translate([5,ard_Y/2,ard_Z])    
        roundcube(3,5,2,0.5,true);
    }
}//
module tft_sd()
{
    //screen
    translate([2.54/-2,2.54/-2,qd_pcb_lift])
    union()
    {
        translate([0,0.4,0])
        union()
        {
            difference()
            {
                union()
                {
                    color("MidnightBlue")
                    cube([qd_pcb_X,qd_pcb_Y,qd_pcb_Z]);
                    

                    translate([3.4,5.6,qd_pcb_Z])
                    union()
                    {
                        difference()
                        {
                            color("white")
                            roundcube(46.9,34.73,qd_lcd_Z,0.2,false);

                            color("blue")
                            translate([1.5,1.5,1])
                            cube([42,32,2.5]);                        
                        }
                        color("black")
                            translate([1.5,1.5,1])
                            cube([38,32,2]);                        
                    }
                }
                translate([0,0,-5])   
                union()
                {
                    translate([5,2,0])                    cylinder(r=1.45,h=10);
                    translate([5,qd_pcb_Y-2,0])           cylinder(r=1.45,h=10);
                    translate([qd_pcb_X-2,2,0])           cylinder(r=1.45,h=10);
                    translate([qd_pcb_X-2,qd_pcb_Y-2,0])  cylinder(r=1.45,h=10);
                }
            }
            //sd card slot
            color("lightgrey")
            translate([14.37,-17.8,-3.2])
            cube([26,36,3.2]);
        }

        
        //header
        translate([0,0,-header_z])
        grid(0,1) pinheader(1,16,1);
    }
}//

module pinheader(x=1, y=1, z=1)
{
    top_len=2.8;
    bot_len=5.8;
    
    color("black")
    translate([2.54/-2 + header_x*x/2, 2.54/-2 + header_y*y/2,header_z*z/2])
    cube([header_x*x,header_y*y,header_z*z],true);

    for (i =[0:x-1])
    for (j =[0:y-1])
    translate([i*2.54, j*2.54, (header_z+bot_len+top_len)/2-bot_len])
    color("grey")
    cube([0.5,0.5,header_z+bot_len+top_len],true);

}//
module pinsocket(x=1, y=1, z=2)
{
    top_len=8.6;
    bot_len=2.54;
    
    color("black")
    translate([2.54/-2 + header_x*x/2, 2.54/-2 + header_y*y/2,top_len/2])
    cube([header_x*x,header_y*y,top_len],true);

    for (i =[0:x-1])
    for (j =[0:y-1])
    translate([i*2.54, j*2.54,bot_len/-2])
    color("grey")
    cube([0.5,0.5,bot_len],true);

}//
module resistor(rotation=0,c1="brown",c2="black",c3="red")
{
    //Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal
    $fn=50;
    rotate([0,0,rotation])
    union()
    {
        translate([0,0,2.5/2])
        rotate([90,0,0])
        union()
        {

            translate([0,0,(10.16-6.3)/2])
            union()
            {   //6.3
                color("BurlyWood")    translate([0,0,0])  cylinder(r=2.45/2,h=6.3);
                color(c1)       translate([0,0,0.5])  cylinder(r=2.5/2,h=1);
                color(c2)       translate([0,0,2])  cylinder(r=2.5/2,h=1);
                color(c3)       translate([0,0,3.5])  cylinder(r=2.5/2,h=1);
            }
            color("grey")
                cylinder(r=0.25,h=10.16);
        }
        translate([0,0,-2])
            color("grey")
            cylinder(r=0.25,h=2.5/2+2);
        translate([0,0,-2])
            color("grey")
            translate([0,-10.16,0])
            cylinder(r=0.25,h=2.5/2+2);
    }
}//


module roundcube(x=10,y=10,z=10,rad=0.2,centered=false)
{
    a=x/2-rad;
    b=y/2-rad;
    c=z/2-rad;
    
    if (centered==true)
    {
        hull()
        {
            translate([a,b,c]) sphere(r=rad);
            translate([-a,b,c]) sphere(r=rad);
            translate([a,-b,c]) sphere(r=rad);
            translate([-a,-b,c]) sphere(r=rad);
            translate([a,b,-c]) sphere(r=rad);
            translate([-a,b,-c]) sphere(r=rad);
            translate([a,-b,-c]) sphere(r=rad);
            translate([-a,-b,-c]) sphere(r=rad);
        }
    }    
    else
    {
        translate([x/2,y/2,z/2])
        hull()
        {
            translate([a,b,c]) sphere(r=rad);
            translate([-a,b,c]) sphere(r=rad);
            translate([a,-b,c]) sphere(r=rad);
            translate([-a,-b,c]) sphere(r=rad);
            translate([a,b,-c]) sphere(r=rad);
            translate([-a,b,-c]) sphere(r=rad);
            translate([a,-b,-c]) sphere(r=rad);
            translate([-a,-b,-c]) sphere(r=rad);
        }
    }        
}//

module led_flat(rotation=0)
{
    translate([-0.25,-0.25,0])
    rotate([0,0,-rotation])
    union()
    {
        color("LimeGreen")
        translate([2.54/-2+0.4,2.54/-2,0])
        union()
        {
            cube([5.06, 2.85, 0.73]);

            translate([0,(2.85-1.95)/2,0])
            cube([4.93,1.95,7.21]);
        }

        color("lightgrey")
        union()
        {
            translate([0,0,-27.4])
            cube([0.5,0.5,27.4]);
            translate([2.54,0,-28.5])
            cube([0.5,0.5,28.5]);
        }
    }    
}//

module pcb(x,y,z)
{
    
    difference()
    {
        cube([x,y,z]);
 
        union()
        {
            for (i =[2.54/2:2.54:x])
                for (j =[2.54/2:2.54:y])
                    translate([i,j,0])
                        cube([0.5,0.5,20],true);
                        //cylinder(r=0.5,h=20);
            
        }
    } 
}//


module push_button(rotation=0,cap=true)
{
    translate([0.35,-3.46,0])
    union()
    {
        difference()
        {
            color("black")
            cube([12,12,3.3]);
            
            translate([6,6,0])
            for (i=[0:90:360])
                color("red")
                rotate([0,0,i])
                translate([7,7,2])
                rotate([0,0,45])
                cube([5,5,5],true);
        }
        
        color("DarkOrange")
        translate([6,6,3.3])
        cylinder(r=6.82/2,h=1);

        color("DarkOrange")
        translate([6,6,3.3+1+0.5])
        cube([2.5,2.5,1],true);

        color("DarkOrange")
        translate([6,6,3.3+1 + 1 + 1])
        cube([3.78,3.78,2],true);
        
        color("grey")
        union()
        {
            translate([-0.35,(12-5.08)/2,0])  cube([0.5,0.5,5],true);
            translate([-0.35,(12-5.08)/2+5.08,0])  cube([0.5,0.5,5],true);
            translate([12.35,(12-5.08)/2,0])  cube([0.5,0.5,5],true);
            translate([12.35,(12-5.08)/2+5.08,0])  cube([0.5,0.5,5],true);
        }
        
        if (cap)
        {
            button_cap();
        }
    }   
}

module button_cap()
{
    translate([0,0,gap])
    difference()
    {
        color("teal")
        translate([6,6,3.3+1+2.54])
        union()
        {
/*
            cylinder(r=11.5/2,h=5.78);
            cylinder(r=12.96/2,h=1.37);
            translate([0,0,2.54/-2])
            cube([5.5,5.5,2.54],true);
*/
            translate([0,0,(9+2.46-gap*2 + wall)/2-2.45])
            roundcube(8,6,9+2.46-gap*2 + wall,0.5,true);
            translate([0,0,2.54/-2])
            cube([5.5,5.5,2.54],true);
            
            translate([0,0,3.5+1.23+(10+2.46-gap*2)/2-3-gap*2])
            cube([9,7,2],true);
         }
         union()
         {

            color("DarkOrange")
                translate([6,6,3.3+1 + 1 + 1 - 1.1])
                cube([3.78+gap*2,3.78+gap*2,4.1],true);
            
            if ($preview)
                color("red")
                translate([5,-5,0])    
                cube([10,10,20]);
             
        }   
     } 
     

}
