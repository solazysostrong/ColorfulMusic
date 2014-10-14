#include <iostream>
#include <stdlib.h>

using namespace std;

#define HISTORY_SIZE 4
float ** g_fftBufs = NULL;
long g_bufferSize = 5;
void initializeFftBufs() {

    g_fftBufs = new float*[HISTORY_SIZE];
      for (int i = 0; i < HISTORY_SIZE; i++) {
            g_fftBufs[i] = new float[g_bufferSize];
                for (int j = 0; j < g_bufferSize; j++) {
                        g_fftBufs[i][j] = 0;
                            }
                  }
    g_fftBufs[0][0] = 1.0;
}

void shiftRightFftBufs(float* current) {
    cout << "A" << endl;
    for (int i = HISTORY_SIZE-1; i > 0; i--) {
      cout << i << " ";
          memmove(&g_fftBufs[i][0], &g_fftBufs[i-1][0], g_bufferSize * sizeof(float));
            }
      for (int i = 0; i < g_bufferSize; i++) {
            g_fftBufs[0][i] = current[i];
              }
}

void printFFT() {
  for (int i = 0; i < HISTORY_SIZE; i++) {
    cout << "i: ";
    for (int j = 0; j < g_bufferSize; j++) {
      cout << g_fftBufs[i][j] << "\t";
    }
    cout << endl << "***********" << endl;
  }
}



int main() {
  initializeFftBufs();
  while(1) {
    printFFT();
    float current[5] = {1,2,3,4,5};
    shiftRightFftBufs(current);
    cout << "----" << endl;
    cin.ignore();
  }

}
