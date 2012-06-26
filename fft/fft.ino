#include <avr/pgmspace.h>
#include <pins_arduino.h>
#define N_WAVE	256             /* full length of Sinewave[] */
#define LOG2_N_WAVE 8           /* log2(N_WAVE) */
#define NUM_FREQS 64            /* number of frequencies to collect */

/*
  fix_fft() - perform forward/inverse fast Fourier transform.
  fr[n],fi[n] are real and imaginary arrays, both INPUT AND RESULT
  (in-place FFT), with 0 <= n < 2**m; set inverse to 0 for forward
  transform (FFT), or 1 for iFFT.
*/
int fix_fft(char fr[], char fi[], int m, int inverse);

char im[128];
char data[128];

int pin_adc = 9;

void setup() {
  // initialize the digital pin as an output.
  // Pin 13 has an LED connected on most Arduino boards:
  pinMode(13, OUTPUT);
  Serial.begin(9600);
}

void send_data(char *data){
  int i;
  Serial.print("[data] " );
  for(i=0;i<NUM_FREQS;++i){
    Serial.print(data[i]);
    if(i<(NUM_FREQS-1))
      Serial.print(" ");
  }
  Serial.println("");
}

void loop(){
  int static i = 0;
  int total = 0;
  static long tt;
  int val;

  if (millis() > tt){
    if (i < 128){
      val = analogRead(pin_adc);
      data[i] = val / 4 - 128;
      im[i] = 0;
      i++;
    }
    else{
      //this could be done with the fix_fftr function without the im array.
      fix_fft(data, im, 7, 0);
      // I am only interessted in the absolute value of the transformation
      for (i=0; i<NUM_FREQS;i++){
        data[i] = sqrt(data[i] * data[i] + im[i] * im[i]);
      }
      // do something with the data values 1..64 and ignore im
      send_data(data);
    }

    tt = millis();
  }
}

/************************
 *
 * FFT implementation
 *
 ************************/

const prog_int8_t Sinewave[N_WAVE-N_WAVE/4] PROGMEM = {
  0, 3, 6, 9, 12, 15, 18, 21,
  24, 28, 31, 34, 37, 40, 43, 46,
  48, 51, 54, 57, 60, 63, 65, 68,
  71, 73, 76, 78, 81, 83, 85, 88,
  90, 92, 94, 96, 98, 100, 102, 104,
  106, 108, 109, 111, 112, 114, 115, 117,
  118, 119, 120, 121, 122, 123, 124, 124,
  125, 126, 126, 127, 127, 127, 127, 127,

  127, 127, 127, 127, 127, 127, 126, 126,
  125, 124, 124, 123, 122, 121, 120, 119,
  118, 117, 115, 114, 112, 111, 109, 108,
  106, 104, 102, 100, 98, 96, 94, 92,
  90, 88, 85, 83, 81, 78, 76, 73,
  71, 68, 65, 63, 60, 57, 54, 51,
  48, 46, 43, 40, 37, 34, 31, 28,
  24, 21, 18, 15, 12, 9, 6, 3,

  0, -3, -6, -9, -12, -15, -18, -21,
  -24, -28, -31, -34, -37, -40, -43, -46,
  -48, -51, -54, -57, -60, -63, -65, -68,
  -71, -73, -76, -78, -81, -83, -85, -88,
  -90, -92, -94, -96, -98, -100, -102, -104,
  -106, -108, -109, -111, -112, -114, -115, -117,
  -118, -119, -120, -121, -122, -123, -124, -124,
  -125, -126, -126, -127, -127, -127, -127, -127,

  /*-127, -127, -127, -127, -127, -127, -126, -126,
    -125, -124, -124, -123, -122, -121, -120, -119,
    -118, -117, -115, -114, -112, -111, -109, -108,
    -106, -104, -102, -100, -98, -96, -94, -92,
    -90, -88, -85, -83, -81, -78, -76, -73,
    -71, -68, -65, -63, -60, -57, -54, -51,
    -48, -46, -43, -40, -37, -34, -31, -28,
    -24, -21, -18, -15, -12, -9, -6, -3, */
};

inline char FIX_MPY(char a, char b){
  //Serial.println(a);
  //Serial.println(b);

  /* shift right one less bit (i.e. 15-1) */
  int c = ((int)a * (int)b) >> 6;
  /* last bit shifted out = rounding-bit */
  b = c & 0x01;
  /* last shift + rounding bit */
  a = (c >> 1) + b;

  /*
    Serial.println(Sinewave[3]);
    Serial.println(c);
    Serial.println(a);
    while(1);*/

  return a;
}

int fix_fft(char fr[], char fi[], int m, int inverse)
{
  int mr, nn, i, j, l, k, istep, n, scale, shift;
  char qr, qi, tr, ti, wr, wi;

  n = 1 << m;

  /* max FFT size = N_WAVE */
  if (n > N_WAVE)
    return -1;

  mr = 0;
  nn = n - 1;
  scale = 0;

  /* decimation in time - re-order data */
  for (m=1; m<=nn; ++m) {
    l = n;
    do {
      l >>= 1;
    } while (mr+l > nn);
    mr = (mr & (l-1)) + l;

    if (mr <= m)
      continue;
    tr = fr[m];
    fr[m] = fr[mr];
    fr[mr] = tr;
    ti = fi[m];
    fi[m] = fi[mr];
    fi[mr] = ti;
  }

  l = 1;
  k = LOG2_N_WAVE-1;
  while (l < n) {
    if (inverse) {
      /* variable scaling, depending upon data */
      shift = 0;
      for (i=0; i<n; ++i) {
        j = fr[i];
        if (j < 0)
          j = -j;
        m = fi[i];
        if (m < 0)
          m = -m;
        if (j > 16383 || m > 16383) {
          shift = 1;
          break;
        }
      }
      if (shift)
        ++scale;
    } else {
      /*
        fixed scaling, for proper normalization -- there will be
        log2(n) passes, so this results in an overall factor of 1/n,
        distributed to maximize arithmetic accuracy.
      */
      shift = 1;
    }
    /*
      it may not be obvious, but the shift will be performed on each
      data point exactly once, during this pass.
    */
    istep = l << 1;
    for (m=0; m<l; ++m) {
      j = m << k;
      /* 0 <= j < N_WAVE/2 */
      wr =  pgm_read_word_near(Sinewave + j+N_WAVE/4);

      /*Serial.println("asdfasdf");
        Serial.println(wr);
        Serial.println(j+N_WAVE/4);
        Serial.println(Sinewave[256]);

        Serial.println("");*/

      wi = -pgm_read_word_near(Sinewave + j);
      if (inverse)
        wi = -wi;
      if (shift) {
        wr >>= 1;
        wi >>= 1;
      }
      for (i=m; i<n; i+=istep) {
        j = i + l;
        tr = FIX_MPY(wr,fr[j]) - FIX_MPY(wi,fi[j]);
        ti = FIX_MPY(wr,fi[j]) + FIX_MPY(wi,fr[j]);
        qr = fr[i];
        qi = fi[i];
        if (shift) {
          qr >>= 1;
          qi >>= 1;
        }
        fr[j] = qr - tr;
        fi[j] = qi - ti;
        fr[i] = qr + tr;
        fi[i] = qi + ti;
      }
    }
    --k;
    l = istep;
  }
  return scale;
}