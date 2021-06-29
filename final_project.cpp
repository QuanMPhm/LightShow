// Copyright 2021 Quan PHam quanmp@bu.edu

#include <iostream>
#include <fstream>
#include <unistd.h>
#include <omp.h>
#include <math.h>
#include <string>
#include <chrono>
#include <map>
#include <set>
#include <tuple>
#include <thread>
#include <vector>

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <fftw3.h>
using std::thread;
using std::cin;
using std::cout;
using std::ifstream;
using std::ios;
using std::streampos;
using std::vector;
using std::string;
using std::map;
using std::tuple;
using std::max;
using std::min;
using std::sort;
using std::pair;

class Channel {
    public:
        signed short * both;
        signed short * channel1;
        signed short * channel2;

        int channelSize;  // Amount of values each channel store
        int songSize;  // Song size in bytes
        int channelCount;  // Amount of channels, max 2


        Channel() {};

        ~Channel() {
            delete[] channel1;
            delete[] channel2;
            delete[] both;
        }

        void init(int channelC, int songS) {
            songSize = songS;
            channelCount = channelC;
            both = new signed short[songSize/sizeof(signed short)];
        }

        // After both has been filled, placed WAV data into their channels
        void setChannels() {
            if (channelCount == 1) {
                channelSize = songSize/sizeof(signed short);
                channel1 = new signed short[channelSize];

                for (int i = 0; i < channelSize; i++) {
                    channel1[i] = both[i];
                }

            } else if (channelCount == 2) {
                channelSize = songSize/(sizeof(signed short)*2);
                channel1 = new signed short[channelSize];
                channel2 = new signed short[channelSize];

                for (int i = 0; i < channelSize; i++) {
                    channel1[i] = both[i*2];
                    channel2[i] = both[i*2 + 1];
                }
            }
        }
};

class Shard {
    public: 
        string note;
        float frequency;
        sf::ConvexShape shape;

        bool active = false;  // Indicate if tile has been activated

        Shard(string n2, float fre, sf::ConvexShape sh) {
            note = n2;
            frequency = fre;
            shape = sh;
        }

        //Partially activate, enable anti-dim
        void activate() {
            active = true;
            shape.setFillColor(sf::Color(255,0,0,50));
            return;
        }

        //  Fully activate tile, disable anti-dim
        void factivate() {
            shape.setFillColor(sf::Color(255,0,0,255));
            active = false;
        }
};

class SongPlayer {
    public:
        string songName;
        int tempo;
        float spb; // Seconds per beat
        float startTime; // Time when "first" beat starts, i.e 1.242s
        const int fps = 40; // FPS locked at 20
        int spf; // Samples to count per frame
        int frameCount = 0; // Number of frames that has elasped
        int sampleRate;
        int channels;
        int bps; // Bits per sample
        int songSize;

        int beatToTake = 8; // Beats we want the user to measure tempo

        //Channels containing raw data
        // signed short * channel1;
        // int channel1Size;
        // signed short * channel2;
        // int channel2Size;

        Channel * channel;

        //fftw stuff
        double * in;
        fftw_complex * out;
        fftw_plan p;
        int frange = 2000;  // Range of frequency to return from fft
        
        bool setSong(string sN) {
            channel = new Channel();
            songName = sN;
            frameCount = 0;
            ifstream ifs;

            ifs.open(songName, ios::binary | ios::in);
            if (ifs.is_open()) {
                cout << "Opened sound file\n";
            } else {
                cout << "Sound file not found\n";
                return false;
            }

            //Check if file is WAVE
            char wave[4];
            ifs.seekg(8);
            ifs.read(wave, 4);
            if (string(wave) != "WAVE") {
                cout << "File not in WAVE format\n";
                return false;
            }

            //Get channels amount
            unsigned short ch[1];
            ifs.seekg(22);
            ifs.read((char *) ch, 2);
            channels = ch[0];

            //Get Sample Rate
            unsigned short smp[1];
            ifs.seekg(24);
            ifs.read((char *) smp, 2);
            sampleRate = smp[0];

            //Get Bits per sample
            unsigned short bp[1];
            ifs.seekg(34);
            ifs.read((char *) bp, 2);
            bps = bp[0];

            //Get Song Size in bytes
            int ss[1];
            ifs.seekg(40);
            ifs.read((char *) ss, 4);
            songSize = ss[0];

            if (songSize < 50'000'000) {
                if (bps == 16) {

                    (*channel).init(channels, songSize);
                    ifs.seekg(44);
                    ifs.read((char *) (*channel).both, songSize);
                    (*channel).setChannels();

                } else if (bps == 8) { cout << "8bps support coming soon\n";
                } else {
                    cout << "Bit rate not supported\n";
                    return false;
                }
            } else {
                cout << "Song file too large\n";
                return false;
            }

            ifs.close();
            spf = sampleRate/fps;
            in = new double[sizeof(double) * sampleRate];
            out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * sampleRate);
            cout << "Song file successfully setup!\n";

            return true;
            
        };
        bool setTempoMan(int temp) {
            tempo = temp;
            spb = (float) 60/tempo;
            cout << "Beat set at: " << tempo << " BPM\n";
            return true;
        }
        bool setTempo() {

            sf::Music music;
            if (!music.openFromFile(songName)) {
                cout << "Song could not be opened\n";
                return false;
            }
            music.play();
            cout << "Playing song...\n";
            cout << "Press " << beatToTake+1 << " times to set the beat";

            //Obtain time between 4 presses
            //Then calculate average time, and bpm
            char temp[1];
            double timeSteps[beatToTake];
            cin.getline(temp, 1);
            cin.getline(temp, 1);
            for (int i = 0; i < beatToTake; i ++) {
                auto start_time = std::chrono::high_resolution_clock::now();
                cin.getline(temp, 1);
                auto end_time = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> fp_ms = end_time - start_time;
                double Step = fp_ms.count() / 1000;
                timeSteps[i] = Step;
                cout << Step << "\n";
            }

            double sum = 0.0;
            for (auto i : timeSteps) sum += i;
            tempo = (int) 1.0/(sum/beatToTake) * 60;
            spb = (float) 60 / tempo;
            cout << "Beat set at: " << tempo << " BPM\n";

            music.stop();
            return true;
        }
        bool setStart() {
            double * tempIn = new double[sizeof(double) * spf];
            fftw_complex * tempOut = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * spf);
            fftw_plan tempP;

            int startHead = 0;
            float avgAmp = 0;

            while (avgAmp < 1000) {
                startHead += spf;
                float sumAmp = 0;
                for (int i = 0; i < spf; i++) {
                    tempIn[i] = (*channel).channel1[startHead + i];
                    if (i >= spf - 100) cout << i << ":" <<  tempIn[i] << " ";
                }

                tempP = fftw_plan_dft_r2c_1d(spf, tempIn, tempOut, FFTW_ESTIMATE);
                fftw_execute(tempP);

                for (int i = 0; i < spf; i++) {
                    float squaresum = pow(tempOut[i][0], 2) + pow(tempOut[i][1], 2); 
                    float mag = pow(squaresum, 0.5);
                    sumAmp += mag;
                }
                avgAmp = sumAmp/spf;
            }

            startTime = (float) (startHead + spf*2)/sampleRate;  // put start time 2 frames ahead. Calibration

            // Free up fftw stuff
            fftw_destroy_plan(tempP);
            fftw_free(tempOut);

            return true;
        }
        // Should be called to update in buffer. Needs to know time, or sf::Vector2f in song
        // Should have two different behavior depending if sound file too large, or small enough
        // Updates internal Complex in buffer
        // Read spf, then copies enough to fill entire second
        void readSongFrame(float songTime) {
            
            for (int i = 0; i < spf*fps; i++) {
                if (i >= spf*2) in[i] = 0;
                else in[i] = (*channel).channel1[i + (int) (songTime * sampleRate)];
            }

            return;
        }; 

        // Given internal in buffer, perform fft, and return amplitude for each frequency
        vector<float> fftSong() {
            p = fftw_plan_dft_r2c_1d(sampleRate, in, out, FFTW_ESTIMATE);
            fftw_execute(p);

            float largest = 0;
            float hz = 0;

            // Sample amplitude from 0Hz to 2000Hz for now
            vector<float> ampCloud;
            for (int i = 0; i < frange; i++) {

                float mag = pow(pow(out[i][0], 2) + pow(out[i][1], 2), 0.5);
                ampCloud.push_back(mag);
            }

            fftw_destroy_plan(p);  // Destroy plan after doing FFT each time
            return ampCloud;
        };
        void close() {
            //fftw_destroy_plan(p);
            //fftw_free(out);
            delete[] in;
            delete channel;
            cout << "Song done\n";
            return;
        }
};

class Drawer {

    private: 
        // Hardcoded frequencies, and associated Shard:
        vector<Shard> f2n;
        //vector<tuple<string, float, sf::ConvexShape>> f2n;
        // Hardcoded Note text mappings
        vector<sf::Text> noteToken;
        // Player is a square
        sf::RectangleShape player;
        // The dimmer color;
        sf::Color dimmer = sf::Color(0,0,0,10);
        // Dim background when player dies/pause/win
        sf::RectangleShape background;
        // Containing shards that were lit
        vector<sf::ConvexShape> litShards;

        // Containing activated notes
        vector<vector<int>> activeNotes;
        // Our only font 
        sf::Font MyFont;

        // Group of private functions for determining if a sf::Vector2f is in a polygon
        // Taken from: https://www.geeksforgeeks.org/how-to-check-if-a-given-sf::Vector2f-lies-inside-a-polygon/

        // Given three colinear sf::Vector2fs p, q, r, the function checks if
        // sf::Vector2f q lies on line segment 'pr'
        bool onSegment(sf::Vector2f p, sf::Vector2f q, sf::Vector2f r)
        {
            if (q.x <= max(p.x, r.x) && q.x >= min(p.x, r.x) &&
                    q.y <= max(p.y, r.y) && q.y >= min(p.y, r.y))
                return true;
            return false;
        }
        
        // To find orientation of ordered triplet (p, q, r).
        // The function returns following values
        // 0 --> p, q and r are colinear
        // 1 --> Clockwise
        // 2 --> Counterclockwise
        int orientation(sf::Vector2f p, sf::Vector2f q, sf::Vector2f r)
        {
            int val = (q.y - p.y) * (r.x - q.x) -
                    (q.x - p.x) * (r.y - q.y);
        
            if (val == 0) return 0; // colinear
            return (val > 0)? 1: 2; // clock or counterclock wise
        }

        bool doIntersect(sf::Vector2f p1, sf::Vector2f q1, sf::Vector2f p2, sf::Vector2f q2) {
            // Find the four orientations needed for general and
            // special cases
            int o1 = orientation(p1, q1, p2);
            int o2 = orientation(p1, q1, q2);
            int o3 = orientation(p2, q2, p1);
            int o4 = orientation(p2, q2, q1);
        
            // General case
            if (o1 != o2 && o3 != o4)
                return true;
        
            // Special Cases
            // p1, q1 and p2 are colinear and p2 lies on segment p1q1
            if (o1 == 0 && onSegment(p1, p2, q1)) return true;
        
            // p1, q1 and p2 are colinear and q2 lies on segment p1q1
            if (o2 == 0 && onSegment(p1, q2, q1)) return true;
        
            // p2, q2 and p1 are colinear and p1 lies on segment p2q2
            if (o3 == 0 && onSegment(p2, p1, q2)) return true;
        
            // p2, q2 and q1 are colinear and q1 lies on segment p2q2
            if (o4 == 0 && onSegment(p2, q1, q2)) return true;
        
            return false; // Doesn't fall in any of the above cases
        }

        bool isInside(vector<sf::Vector2f> polygon, int n, sf::Vector2f p) {
            // There must be at least 3 vertices in polygon[]
            if (n < 3) return false;
        
            // Create a sf::Vector2f for line segment from p to infinite
            sf::Vector2f extreme(2005, p.y);
        
            // Count intersections of the above line with sides of polygon
            int count = 0, i = 0;
            do
            {
                int next = (i+1)%n;
        
                // Check if the line segment from 'p' to 'extreme' intersects
                // with the line segment from 'polygon[i]' to 'polygon[next]'
                if (doIntersect(polygon[i], polygon[next], p, extreme))
                {
                    // If the sf::Vector2f 'p' is colinear with line segment 'i-next',
                    // then check if it lies on segment. If it lies, return true,
                    // otherwise false
                    if (orientation(polygon[i], p, polygon[next]) == 0)
                    return onSegment(polygon[i], p, polygon[next]);
        
                    count++;
                }
                i = next;
            } while (i != 0);
        
            // Return true if count is odd, false otherwise
            return count&1; // Same as (count%2 == 1)
        }

        // Comparator function for soring pair vectors
        static bool peakcmp(pair<float, float>& a, pair<float, float>& b) {
            return a.second > b.second;
        }
    public:

        int px, py;
        int pspeed = 5;  // Player speed in pixels
        int bdelay = 2;  // Beats to delay before they appear
        int lives = 3;  // Player starts with 3 lives

        //Initialize Drawer with shards
        Drawer(){

            vector<string> notes3 = {"A","B","C","D","D#","F","F#","G#"};
            vector<float> freqs3 = {110.0,123.47,130.81, 146.83, 155.56,174.61, 185.00,207.65};
            //Coordinates of sf::Vector2fs of 3-sf::Vector2f shards, then position
            vector<sf::Vector2f> shard3Coor = {
                sf::Vector2f(0, 0), sf::Vector2f(0, 300), sf::Vector2f(500, 300), sf::Vector2f(0, 200), //A
                sf::Vector2f(0, 0), sf::Vector2f(300, 0), sf::Vector2f(300, 500), sf::Vector2f(200, 0),  //A Sharp
                sf::Vector2f(0, 0), sf::Vector2f(300, 0), sf::Vector2f(0, 500), sf::Vector2f(500, 0),
                sf::Vector2f(500, 0), sf::Vector2f(500, 300), sf::Vector2f(0, 300), sf::Vector2f(500, 200),
                sf::Vector2f(0, 0), sf::Vector2f(500, 0), sf::Vector2f(500, 300), sf::Vector2f(500, 500),
                sf::Vector2f(0, 0), sf::Vector2f(0, 500), sf::Vector2f(300, 500), sf::Vector2f(500, 500), //F
                sf::Vector2f(300, 0), sf::Vector2f(300, 500), sf::Vector2f(0, 500), sf::Vector2f(200, 500),             
                sf::Vector2f(0, 0), sf::Vector2f(500, 0), sf::Vector2f(0, 300), sf::Vector2f(0, 500)
            };

            vector<string> notes4 = {"A#", "C#", "E", "G"};
            vector<float> freqs4 = {116.54, 138.59, 164.81, 196.00};
            vector<int> shard4Coor = {
                0,0,200,0,500,500,0,200,0,0,
                300,0,500,0,500,200,0,500,500,0,
                0,0,500,300,500,500,300,500,500,500,
                500,0,200,500,0,500,0,300,0,500
            };

            //Get font
            if (!MyFont.loadFromFile("/usr/share/fonts/truetype/ubuntu/Ubuntu-BI.ttf")) {
                cout << "Can't Font File\n";
            }

            //Init 3 sf::Vector2f shards
            for (int i = 0; i < 8; i++){
                sf::ConvexShape shard;
                shard.setPointCount(3);
                shard.setPoint(0, shard3Coor.at(i*4));
                shard.setPoint(1, shard3Coor.at(i*4 + 1));
                shard.setPoint(2, shard3Coor.at(i*4 + 2));
                shard.setFillColor(sf::Color::Red);
                shard.setPosition(shard3Coor.at(i*4 + 3));
                f2n.push_back(Shard(notes3.at(i), freqs3.at(i), shard));
            }
            //cout << "Finished 3P shards\n";

            //Init 4 sf::Vector2f shards
            for (int i = 0; i < 4; i++) {
                sf::ConvexShape shard;
                shard.setPointCount(4);
                shard.setPoint(0, sf::Vector2f(shard4Coor.at(i*10 + 0), shard4Coor.at(i*10 + 1)));
                shard.setPoint(1, sf::Vector2f(shard4Coor.at(i*10 + 2), shard4Coor.at(i*10 + 3)));
                shard.setPoint(2, sf::Vector2f(shard4Coor.at(i*10 + 4), shard4Coor.at(i*10 + 5)));
                shard.setPoint(3, sf::Vector2f(shard4Coor.at(i*10 + 6), shard4Coor.at(i*10 + 7)));
                shard.setFillColor(sf::Color::Red);
                shard.setPosition(shard4Coor.at(i*10 + 8), shard4Coor.at(i*10 + 9));
                f2n.push_back(Shard(notes4.at(i), freqs4.at(i), shard));
            }
            //cout << "Finished 4P shards\n";

            vector<int> token3 = { 
                100, 400, 400, 100, 600, 100, 900, 400,
                900, 600, 600, 900, 400, 900, 100, 600
            };

            for (int i = 0; i < 8; i++) {
                sf::Text tok;
                tok.setFont(MyFont);
                tok.setCharacterSize(40);
                tok.setFillColor(sf::Color::White);
                tok.setString(notes3.at(i));
                sf::FloatRect trect = tok.getLocalBounds();
                tok.setOrigin(trect.left + trect.width/2.0f, trect.top  + trect.height/2.0f);  // set Text origin to its center
                tok.setPosition(token3.at(i*2), token3.at(i*2 + 1));
                noteToken.push_back(tok);
            }

            vector<int> token4 {150, 150, 850, 150, 850, 850, 150, 850};

            for (int i = 0; i < 4; i++) {
                sf::Text tok;
                tok.setFont(MyFont);
                tok.setCharacterSize(40);
                tok.setFillColor(sf::Color::White);
                tok.setString(notes4.at(i));
                sf::FloatRect trect = tok.getLocalBounds();
                tok.setOrigin(trect.left + trect.width/2.0f, trect.top  + trect.height/2.0f);  // set Text origin to its center
                tok.setPosition(token4.at(i*2), token4.at(i*2 + 1));
                noteToken.push_back(tok);
            }

            //Init player
            player.setSize(sf::Vector2f(30,30));
            player.setFillColor(sf::Color(0,0,255,255));
            player.setOrigin(5,5);
            player.setPosition(500,500);
            px = 500; py = 500;

            //Init background
            background.setSize(sf::Vector2f(1000,1000));
            background.setFillColor(sf::Color(0,0,0,50));
            background.setPosition(0,0);

            // Init activeNotes with delay of 2 beats
            for (int i = 0; i < bdelay; i++) {
                vector<int> temp = {-1};
                activeNotes.push_back(temp);
            }

            //cout << "Finished Player\nFinished initializing Drawer\n";
        }
        //Called every frame to do stuff;
        void updateFrame() {

            // Dim color
            for (int i = 0; i < f2n.size(); i++) {
                Shard shard = f2n.at(i);
                sf::Color fcolor = shard.shape.getFillColor();
                if (shard.active == false) {
                    fcolor -= dimmer;
                } else {
                    if (fcolor.a > 50) fcolor -= dimmer;  // If tile is active, only dim to 50
                }

                f2n.at(i).shape.setFillColor(fcolor);
            }


        };
        //Called every beat to do stuff
        void updateBeat() {

            // Fully lit any active tiles
            vector<int> litI = activeNotes.at(0);
            for (int i : litI) {
                if (i == -1) break;
                f2n.at(i).factivate();
                litShards.push_back(f2n.at(i).shape);
            }
            // Then remove from queue
            activeNotes.erase(activeNotes.begin());
            return;

        }
        //Called whenever there's a beat, activate Shards
        void updateFFT(vector<float> ampCloud) {

            //Obtain freq with highest amplitude

            vector<pair<float, float>> peakspair;  // <freq, amp>
            vector<float> highestFs;
            //vector<float> highestAs;

            for (int f = 108; f < ampCloud.size(); f++) {  // f starts from ~110 because NO BASS
                if (f != ampCloud.size() - 1) {
                    if (ampCloud.at(f) > ampCloud.at(f-1) && ampCloud.at(f) > ampCloud.at(f+1)) 
                        peakspair.push_back(std::make_pair(f, ampCloud.at(f)));
                }
            }

            sort(peakspair.begin(), peakspair.end(), peakcmp);
            peakspair.resize(2);

            // If the second highest peak is not 80% of the top, don't activate its tile
            if (peakspair.at(1).second/peakspair.at(0).second < 0.8) peakspair.resize(1);

            // Obtain the 2 highest peaks
            for (auto i : peakspair) {
                if (i.second > 100'000)  // Note must meet a min amplitude
                    highestFs.push_back(i.first);
            }

            // Reduce highest F to fundamental to range 110 (1st A) - 207 (2nd A)
            for (int i = 0; i < highestFs.size(); i++) {
                while (highestFs.at(i) > 211.0) {
                    highestFs.at(i) /= 2;
                }
            }

            //If frequency matches, obtain note and break
            vector<int> acti;
            string note;

            for (auto highestF : highestFs) {
                for (int i = 0; i < f2n.size(); i++) {
                    Shard shard = f2n.at(i);
                    float notef = shard.frequency;

                    // Obtain discrepancy between highest freq. and actual freq.
                    float discrep;
                    if (highestF > notef) discrep = fmod(highestF, notef);
                    else discrep = fmod(notef, highestF);

                    if (discrep/notef < 0.028) {  // If discrepancy is less than 2.8% of note freq. 
                        note = shard.note;
                        acti.push_back(i);
                        f2n.at(i).activate();  // Dim lit tile
                        //litShards.push_back(f2n.at(i).shape); // Add tiles to lit tiles
                        break;
                    }
                }

                //cout << "Highest freq: " << highestF << " " << note << "\n";
            }

            if (acti.size() == 0) acti.push_back(-1);
            activeNotes.push_back(acti);
        };
        //Called when movement keys are pressed
        void updatePlayer(char move) {
            // Prevent player from going out of bounds
            if (move == 'd' && py < 1000) py += pspeed;
            if (move == 'u' && py > 0) py -= pspeed;
            if (move == 'l' && px > 0) px -= pspeed;
            if (move == 'r' && px < 1000) px += pspeed;
            player.setPosition(px, py);

            if (lives == 2) {
                player.setSize(sf::Vector2f(20, 20));
                player.setFillColor(sf::Color(0,150,150,255));
            }

            if (lives == 1) {
                player.setSize(sf::Vector2f(15, 15));
                player.setFillColor(sf::Color(150,150,0,255));
                pspeed = 10;
            }
        }
        //Called when alive, to check if player is dead or not. True for dead
        bool isDead() {
            sf::Vector2f player(px, py);
            for (auto shard : litShards) {
                int pcount = shard.getPointCount();
                vector<sf::Vector2f> pvec;
                for (int i = 0; i < pcount; i++) {
                    sf::Vector2f abpos = shard.getPosition();  // Get absolute position of tile
                    sf::Vector2f p = shard.getPoint(i);
                    p += abpos;  // Obtain absolute coordinate for each point
                    pvec.push_back(p);
                }
                if (isInside(pvec, pcount, player)) lives--;  // If user is hit, returns DEAD
                if (lives == 0) return true;
            }
            litShards.clear();
            //cout << "Lives " << lives << "\n";
            return false;
        }

        // ORDER MATTERS WHEN DRAWING. BACKGROUND FIRST!!!!
        sf::Text getUText(string status) {
            sf::Text statText;
            statText.setFont(MyFont);

            if (status == "Win") {
                statText.setString("Winner Winner\nQ to quit");
                statText.setStyle(sf::Text::Bold);
                statText.setFillColor(sf::Color::Yellow);                
            } else if (status == "Dead") {
                statText.setString("You're Dead - Q to quit");
                statText.setStyle(sf::Text::Bold);
                statText.setFillColor(sf::Color::Blue);
            } else if (status == "Pause") {
                statText.setString("Paused - P to continue\nQ to quit");
                statText.setStyle(sf::Text::Bold);
                statText.setFillColor(sf::Color::Green);
            }


            statText.setCharacterSize(50);
            sf::FloatRect trect = statText.getLocalBounds();
            statText.setOrigin(trect.left + trect.width/2.0f, trect.top  + trect.height/2.0f);
            statText.setPosition(500,500);


            return statText;
        }

        //Get functions for drawing
        vector<sf::ConvexShape> getShards() {
            vector<sf::ConvexShape> res;
            for (auto s : f2n) res.push_back(s.shape);
            return res;
        }
        
        vector<sf::Text> getTokens() {return noteToken;};
        sf::RectangleShape getPlayer() {return player;};
        sf::RectangleShape getBackground() {return background;};

        //Returns the soundcloud. Only used for debugging FFT
        vector<sf::RectangleShape> getFFT(vector<float> ampCloud) {
            vector<sf::RectangleShape> soundCloud;

            int highestf = 0;
            float highestA = 0;
            for (int i = 0; i < ampCloud.size(); i++) {
                float amp = ampCloud.at(i);
                float normAmp = amp/10000;  // Reduce amp
                sf::RectangleShape sound;
                sound.setPosition(i%1000, 10 + 500*(i/1000)); // Make rows of 1000 each
                sound.setSize(sf::Vector2f(1, normAmp));
                if (i % 20 == 0) sound.setFillColor(sf::Color::Red);
                if (i % 100 == 0) sound.setFillColor(sf::Color::Blue);
                soundCloud.push_back(sound);

                if (ampCloud.at(i) > highestA) {
                    highestA = ampCloud.at(i);
                    highestf = i;
                }
            }

            //cout << "Highest freq: " << highestf << "\n";
            return soundCloud;
        }

};

int main(){

    float beatNext; // Contains the time of the next beat, 
    float songEnd; // Contains the time of end of song
    float beatTime; // Duration between beats

    SongPlayer sp;
    Drawer drw = Drawer();
    cout << "Welcome to LightShow\nBefore you play, set a song, tempo, and starting sf::Vector2f.\n";
    cout << "Set [Song Name]"" - Sets a WAV sound file in current directory as your song\n";
    cout << "STempo - Plays song, set tempo based on your key presses\n";
    cout << "MTempo [Tempo] - Type in your BPM, reconmended if you know the actual BPM of the song\n";
    cout << "Time - Plays song again, and you press enter to set starting time\n";
    cout << "Quit"" - Quit game\n";
    cout << "Start"" - Start playing only after setting song, tempo, and start time\n";

    bool setsong = false;
    bool settempo = false;
    bool setstart = false;

    bool debug = false;  // Toggle debug mode, displays FFT spectrum
    int dbeat = drw.bdelay;  // Get beats to read ahead in the futre, THIS IS A MISTAKE

    string cmd;
    while (cin >> cmd) {
        if (cmd == "Set") {
            string sN;
            cin >> sN;
            setsong = sp.setSong(sN);
        } else if (cmd == "STempo") {
            if (setsong) {
                settempo = sp.setTempo();
                beatTime = sp.spb; // Get beat time
            }
            else cout << "Song not yet set!\n";
        }  else if (cmd == "MTempo") {
            if (setsong) {
                int tem;
                cin >> tem;
                settempo = sp.setTempoMan(tem);
                beatTime = sp.spb; // Get beat time
            }
            else cout << "Song not yet set!\n";
        } else if (cmd == "Time") {
            if (setsong) {
                setstart = sp.setStart();
                beatNext = sp.startTime; // Init start time as first beat
            }
            else cout << "Song not yet set!\n";
        } else if (cmd == "Quit") {
            cout << "Goodbye\n";
            return 0;
        } else if (cmd == "Start") {
            if (setsong && settempo && setstart) {
                
                beatNext = sp.startTime;  // Reset beat time
                drw.lives = 3;  // Reset lives

                sf::Music music;
                music.openFromFile(sp.songName);
                music.setLoop(false);
                music.play();

                sf::RenderWindow window(sf::VideoMode(1000, 1000), "FFT TEST");
                window.setFramerateLimit(sp.fps);
                window.setKeyRepeatEnabled(false);

                songEnd = music.getDuration().asSeconds(); // We need to know when song ends
                string gState = "Alive";  // Tracking game state, start as Alive
                //cout << "Song end at " << songEnd << "\n";
                cout << "Playing song for game\n";
                cout << "Opening window...\n";

                while (window.isOpen()) {
                    float songTime = music.getPlayingOffset().asSeconds();
                    // Event pool to smoothly close window

                    if (music.getStatus() == music.Stopped) gState = "Win";  // If next beat beyond song time, it's over!

                    if (gState == "Alive") {
                        if (music.getStatus() == sf::Music::Paused) music.play();
                        if (!debug) window.clear();
                    
                        // When time in song after song-start, and before song-ends
                        if (songTime > sp.startTime && songTime < songEnd) {
                            // Check when time is past the beat time
                            if (songTime >= beatNext) {

                                //Making sure we're not going out of bounds
                                if (songTime < songEnd-dbeat*beatTime) {
                                    //cout << "Current time " << songTime << " Reading from: " << beatNext + dbeat*beatTime << "\n";
                                    sp.readSongFrame(beatNext + dbeat*beatTime);  // Read in samples 2 beats ahead of time
                                    vector<float> ampCloud = sp.fftSong(); // Obtain fft transform, and amp for 2000 Frequencies

                                    if (debug) {  // Draw the rectangle based on ampCloud
                                        window.clear();
                                        for (auto sound : drw.getFFT(ampCloud)) window.draw(sound);
                                    } else {
                                        drw.updateFFT(ampCloud);  // Lit up shards
                                        drw.updateBeat();  // Update the internal queues
                                        if (drw.isDead()) gState = "Dead";  // Check if player is dead
                                    } 
                                }

                                beatNext += beatTime; // Update to next beat
                            }
                        }

                        // If we're not in debug mode, dispaly normal game objects
                        if (!debug) {
                            
                            drw.updateFrame(); // Called every frame when alive

                            // Keyboard poll for movement
                            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left)) {
                                drw.updatePlayer('l');
                            }
                            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right)) {
                                drw.updatePlayer('r');
                            }
                            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up)) {
                                drw.updatePlayer('u');
                            }
                            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down)) {                                
                                drw.updatePlayer('d');
                            }

                            // // Main() SHOULD NOT BE HANDLING SHARD DRAWING
                            // drw.dimShard();
                            // Draw shards
                            for (auto shard : drw.getShards()) window.draw(shard);
                            // Draw tokens
                            for (auto token : drw.getTokens()) window.draw(token);
                            // Draw player
                            window.draw(drw.getPlayer());
                        }
                    } else if (gState == "Pause") {
                        window.clear();
                        window.draw(drw.getBackground());
                        window.draw(drw.getUText(gState));
                        music.pause();
                    } else if (gState == "Win") {
                        //window.draw(drw.getBackground());
                        window.draw(drw.getUText(gState));
                    } else if (gState == "Dead") {
                        //window.draw(drw.getBackground());
                        window.draw(drw.getUText(gState));
                    }

                    sf::Event event;
                    while (window.pollEvent(event)) {  
                        switch (event.type) {
                            case sf::Event::Closed:
                                music.stop();
                                window.close();
                                cout << "Game closed\n";
                                break;
                            case sf::Event::KeyPressed:
                                if (event.key.code == sf::Keyboard::P) {
                                    if (gState == "Alive") {
                                        gState = "Pause";
                                        break;
                                    }
                                    if (gState == "Pause") {
                                        gState = "Alive";
                                        break;
                                    }
                                } 

                                if (event.key.code == sf::Keyboard::Q) {
                                    if (gState == "Pause" || gState == "Win" || gState == "Dead") {
                                        music.stop();
                                        window.close();
                                        cout << "Game closed\n";
                                        break;
                                    }
                                }
                            default:
                                break;
                        }   
                    }

                    window.display();
                }

            } else {
                cout << "Everything hasn't been setup yet!\n";
            }
        } else {
            cout << "Invalid Command\n";
        }

    }
    return 0;
}