#include <Arduino_HS300x.h>
#include <Arduino_APDS9960.h>
#include <Arduino_BMI270_BMM150.h>

// ---------- baseline ----------
float brh = 0;
float btemp = 0;
float bmag = 0;
int br = 0, bg = 0, bb = 0, bc = 0;

// ---------- thresholds ----------
const float rhDiff = 3.0;
const float tempDiff = 1.0;
const float magDiff = 8.0;
const int colorDiffTh = 40;

// ---------- current ----------
float rh = 0;
float temp = 0;
float mag = 0;
int r = 0, g = 0, b = 0, c = 0;

// ---------- helper ----------
float getMag() {
    float x, y, z;

    if (IMU.magneticFieldAvailable()) {
        IMU.readMagneticField(x, y, z);
        return sqrt(x * x + y * y + z * z);
    }

    return mag;
}

void getColor() {
    if (APDS.colorAvailable()) {
        APDS.readColor(r, g, b, c);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    if (!HS300x.begin()) {
        Serial.println("HS300x failed");
        while (1);
    }

    if (!APDS.begin()) {
        Serial.println("APDS failed");
        while (1);
    }

    if (!IMU.begin()) {
        Serial.println("IMU failed");
        while (1);
    }

    // ---------- auto calibration ----------
    float srh = 0;
    float stemp = 0;
    float smag = 0;
    long sr = 0, sg = 0, sb = 0, sc = 0;

    int n = 30;

    for (int i = 0; i < n; i++) {
        delay(150);

        srh += HS300x.readHumidity();
        stemp += HS300x.readTemperature();
        smag += getMag();

        getColor();

        sr += r;
        sg += g;
        sb += b;
        sc += c;
    }

    brh = srh / n;
    btemp = stemp / n;
    bmag = smag / n;

    br = sr / n;
    bg = sg / n;
    bb = sb / n;
    bc = sc / n;
}

void loop() {
    // ---------- read sensors ----------
    rh = HS300x.readHumidity();
    temp = HS300x.readTemperature();
    mag = getMag();
    getColor();

    // ---------- flags ----------
    int humid_jump = (rh > brh + rhDiff) ? 1 : 0;
    int temp_rise = (temp > btemp + tempDiff) ? 1 : 0;
    int mag_shift = (abs(mag - bmag) > magDiff) ? 1 : 0;

    int color_change =
        abs(r - br) +
        abs(g - bg) +
        abs(b - bb) +
        abs(c - bc);

    int light_or_color_change = (color_change > colorDiffTh) ? 1 : 0;

    // ---------- exact required labels ----------
    String label;

    if (humid_jump && temp_rise) {
        label = "BREATH_OR_WARM_AIR_EVENT";
    }
    else if (mag_shift) {
        label = "MAGNETIC_DISTURBANCE_EVENT";
    }
    else if (light_or_color_change) {
        label = "LIGHT_OR_COLOR_CHANGE_EVENT";
    }
    else {
        label = "BASELINE_NORMAL";
    }

    // ---------- required output ----------
    Serial.print("raw,rh=");
    Serial.print(rh, 2);
    Serial.print(",temp=");
    Serial.print(temp, 2);
    Serial.print(",mag=");
    Serial.print(mag, 2);
    Serial.print(",r=");
    Serial.print(r);
    Serial.print(",g=");
    Serial.print(g);
    Serial.print(",b=");
    Serial.print(b);
    Serial.print(",clear=");
    Serial.println(c);

    Serial.print("flags,humid_jump=");
    Serial.print(humid_jump);
    Serial.print(",temp_rise=");
    Serial.print(temp_rise);
    Serial.print(",mag_shift=");
    Serial.print(mag_shift);
    Serial.print(",light_or_color_change=");
    Serial.println(light_or_color_change);

    Serial.print("event,");
    Serial.println(label);

    delay(500);
}
