- Lightshow

An attempt of making a music-based semi-real-time 2-D game using Linux SFML and Discrete Fourier Transform provided by fftw3. 
The program currently only supports WAV files with 8-bit or 16-bit encoding.
The repo contains a demo sound file "Scale2.wav".

DEMO

After running the executable, copy these demo commands to play the game with the demo sound file:

Set Scale2.wav
MTempo 180
Time
Start

COMPILE

To compile using g++:

g++ final_project.cpp -lsfml-audio -lsfml-graphics -lfftw3 -lm -lsfml-window -lsfml-system  -pthread