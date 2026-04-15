#include <PDM.h>
#include <Arduino_APDS9960.h>
#include <Arduino_BMI270_BMM150.h>

short micBuf[256];
volatile int micCount = 0;
int micVal = 0;

int micNow = 0, micPrev = 0;
int clearNow = 0, clearPrev = 0;
int proxNow = 0, proxPrev = 0;
float moveNow = 0.0, movePrev = 0.0;

int micFilt = 0, clearFilt = 0, proxFilt = 0;
float moveFilt = 0.0;

// delta cutoffs
const int micDeltaTh = 12;
const int clearDeltaTh = 18;
const int proxDeltaTh = 4;
const float moveDeltaTh = 0.08;

// flags
int soundFlag = 0;
int darkFlag = 0;
int moveFlag = 0;
int nearFlag = 0;

// label debounce
String lastLabel = "QUIET_BRIGHT_STEADY_FAR";
String candLabel = "QUIET_BRIGHT_STEADY_FAR";
int holdCount = 0;
const int holdNeed = 2;

void onPDMdata() {
    int bytes = PDM.available();
    PDM.read(micBuf, bytes);
    micCount = bytes / 2;
}

int readMic() {
    if (micCount > 0) {
        long sum = 0;
        for (int i = 0; i < micCount; i++) {
            sum += abs(micBuf[i]);
        }
        micVal = sum / micCount;
        micCount = 0;
    }
    return micVal;
}

int readClear() {
    int r, g, b, c;
    if (APDS.colorAvailable()) {
        APDS.readColor(r, g, b, c);
        return c;
    }
    return clearFilt;
}

int readProx() {
    if (APDS.proximityAvailable()) {
        return APDS.readProximity();
    }
    return proxFilt;
}

float readMove() {
    float x, y, z;
    if (IMU.accelerationAvailable()) {
        IMU.readAcceleration(x, y, z);
        return abs(x) + abs(y) + abs(z - 1.0);
    }
    return moveFilt;
}

void setup() {
    Serial.begin(115200);
    delay(1500);

    PDM.onReceive(onPDMdata);

    if (!PDM.begin(1, 16000)) {
        Serial.println("Mic failed");
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

    delay(1000);

    micFilt = readMic();
    clearFilt = readClear();
    proxFilt = readProx();
    moveFilt = readMove();

    micPrev = micFilt;
    clearPrev = clearFilt;
    proxPrev = proxFilt;
    movePrev = moveFilt;
}

void loop() {
    micNow = readMic();
    clearNow = readClear();
    proxNow = readProx();
    moveNow = readMove();

    // smoothing
    micFilt = (2 * micFilt + micNow) / 3;
    clearFilt = (3 * clearFilt + clearNow) / 4;
    proxFilt = (3 * proxFilt + proxNow) / 4;
    moveFilt = (3.0 * moveFilt + moveNow) / 4.0;

    int dMic = micFilt - micPrev;
    int dClear = clearFilt - clearPrev;
    int dProx = proxFilt - proxPrev;
    float dMove = moveFilt - movePrev;

    // delta-based flags
    soundFlag = (dMic > micDeltaTh) ? 1 : 0;
    darkFlag = (dClear < -clearDeltaTh) ? 1 : 0;
    moveFlag = (abs(dMove) > moveDeltaTh) ? 1 : 0;
    nearFlag = (dProx > proxDeltaTh) ? 1 : 0;

    String nowLabel;

    if (!soundFlag && !darkFlag && !moveFlag && !nearFlag) {
        nowLabel = "QUIET_BRIGHT_STEADY_FAR";
    }
    else if (soundFlag && !darkFlag && !moveFlag && !nearFlag) {
        nowLabel = "NOISY_BRIGHT_STEADY_FAR";
    }
    else if (!soundFlag && darkFlag && !moveFlag && nearFlag) {
        nowLabel = "QUIET_DARK_STEADY_NEAR";
    }
    else if (soundFlag && !darkFlag && moveFlag && nearFlag) {
        nowLabel = "NOISY_BRIGHT_MOVING_NEAR";
    }
    else {
        nowLabel = lastLabel;
    }

    if (nowLabel == candLabel) {
        holdCount++;
    } else {
        candLabel = nowLabel;
        holdCount = 1;
    }

    if (holdCount >= holdNeed) {
        lastLabel = candLabel;
    }

    Serial.print("raw,mic=");
    Serial.print(micFilt);
    Serial.print(",clear=");
    Serial.print(clearFilt);
    Serial.print(",motion=");
    Serial.print(moveFilt, 3);
    Serial.print(",prox=");
    Serial.println(proxFilt);

    Serial.print("flags,sound=");
    Serial.print(soundFlag);
    Serial.print(",dark=");
    Serial.print(darkFlag);
    Serial.print(",moving=");
    Serial.print(moveFlag);
    Serial.print(",near=");
    Serial.println(nearFlag);

    Serial.print("state,");
    Serial.println(lastLabel);

    micPrev = micFilt;
    clearPrev = clearFilt;
    proxPrev = proxFilt;
    movePrev = moveFilt;

    delay(250);
}
