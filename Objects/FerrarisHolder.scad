$fn = 100;

LED_DIAMETER = 5;       // in mm
SENSOR_DIAMETER = 3;    // in mm
SENSOR_DISTANCE = 18;   // in mm

LENGTH = 45;            // in mm
WIDTH = 10;             // in mm
HEIGHT = 6;             // in mm

difference() {
    union() {
        translate([-LENGTH / 2, -WIDTH / 2])
            cube([LENGTH, WIDTH, HEIGHT]);
        translate([-10, -20])
            cube([20, 40, 2]);
    }
    translate([SENSOR_DISTANCE / 2, 0])
        rotate([0, 45, 0])
        union(){
            translate([0, 0, -100])
                cylinder(h=200, r=SENSOR_DIAMETER / 2);  // IR fototransistor
            translate([0, 0, 5])
                cylinder(h=100, r=SENSOR_DIAMETER / 2 + 1);
        }

    translate([-SENSOR_DISTANCE / 2, 0])
        rotate([0, -45, 0])
        union(){
            translate([0, 0, -100])
                cylinder(h=200, r=LED_DIAMETER / 2);  // IR fototransistor
            translate([0, 0, 5])
                cylinder(h=100, r=LED_DIAMETER / 2 + 1);
        }
}
