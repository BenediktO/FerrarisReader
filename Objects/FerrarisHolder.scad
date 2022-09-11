$fn = 100;

difference() {
    union() {
        translate([-45 / 2, -5])
            cube([45, 10, 6]);
        translate([-10, -20])
            cube([20, 40, 2]);
    }
    translate([9, 0])
        rotate([0, 45, 0])
        union(){
            translate([0, 0, -10])
                cylinder(h=20, r=3 / 2);  // IR fototransistor
            translate([0, 0, 5])
                cylinder(h=10, r=5 / 2);
        }
        
    translate([-9, 0])
        rotate([0, -45, 0])
        union(){
            translate([0, 0, -10])
                cylinder(h=20, r=5 / 2);  // IR fototransistor
            translate([0, 0, 5])
                cylinder(h=10, r=7 / 2);
        }
}
