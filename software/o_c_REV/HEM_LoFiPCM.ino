// Copyright (c) 2018, Jason Justian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#define HEM_LOFI_PCM_BUFFER_SIZE 2048
#define HEM_LOFI_PCM_SPEED 4
// #define CLIPLIMIT 6144 // 4V
#define CLIPLIMIT HEMISPHERE_3V_CV

const uint8_t BURST_ICON[8]      = {0x11,0x92,0x54,0x00,0xd7,0x00,0x54,0x92};
const uint8_t GAUGE_ICON[8]      = {0x38,0x44,0x02,0x32,0x32,0x0a,0x44,0x3a};

class LoFiPCM : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "LoFi Echo";
    }

    void Start() {
        countdown = HEM_LOFI_PCM_SPEED;
        for (int i = 0; i < HEM_LOFI_PCM_BUFFER_SIZE; i++) pcm[i] = 0;
        cursor = 1; //for gui
    }

    void Controller() {
        play = !Gate(0); // Continuously play unless gated
        fdbk_g = Gate(1) ? 100 : feedback; // Feedback = 100 when gated

        if (play) {
            if (--countdown == 0) {
                if (++head >= length) {
                    head = 0;
                    //ClockOut(1);
                }

                int16_t cv = In(0);
                int cv2 = DetentedIn(1);

                // bitcrush the input
                cv = cv >> depth;
                cv = cv << depth;

                // int dt = dt_pct * length / 100; //convert delaytime to length in samples 
                head_w = (head + length + dt_pct*length/100) % length; //have to add the extra length to keep modulo positive in case delaytime is neg

                // mix input into the buffer ahead, respecting feedback
                pcm[head_w] = constrain((pcm[head] * fdbk_g / 100 + cv), -CLIPLIMIT, CLIPLIMIT);
                
                Out(0, pcm[head]);
                Out(1, pcm[length-1 - head]); // reverse buffer!

                rate_mod = constrain( rate + Proportion(cv2, HEMISPHERE_MAX_CV, 32), 1, 64);
                countdown = rate_mod;
            }
        }
    }

    void View() {
        gfxHeader(applet_name());
        DrawSelector();
        if (play) DrawWaveform();
    }

    void OnButtonPress() {
        if (++cursor > 3) cursor = 0;
        ResetCursor();
    }

    void OnEncoderMove(int direction) {
        switch (cursor) {
        case 0:
            dt_pct = constrain(dt_pct += direction, 0, 99);
            break;
        case 1:
            feedback = constrain(feedback += direction, 0, 125);
            break;
        case 2:
            rate = constrain(rate += direction, 1, 32);
            break;
        case 3:
            depth = constrain(depth += direction, 0, 13);
            break;
        }

        //amp_offset_cv = Proportion(amp_offset_pct, 100, HEMISPHERE_MAX_CV);
        //p[cursor] = constrain(p[cursor] += direction, 0, 100);
    }

    uint32_t OnDataRequest() {
        uint32_t data = 0;
        Pack(data, PackLocation {0,7}, dt_pct);
        Pack(data, PackLocation {7,7}, feedback);
        Pack(data, PackLocation {14,5}, rate);
        Pack(data, PackLocation {19,4}, depth);
        return data;
    }

    void OnDataReceive(uint32_t data) {
        dt_pct = Unpack(data, PackLocation {0,7});
        feedback = Unpack(data, PackLocation {7,7});
        rate = Unpack(data, PackLocation {14,5});
        depth = Unpack(data, PackLocation {19,4});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Pause 2= Fb=100";
        help[HEMISPHERE_HELP_CVS]      = "1=Audio 2=RateMod";
        help[HEMISPHERE_HELP_OUTS]     = "A=Audio B=Reverse";
        help[HEMISPHERE_HELP_ENCODER]  = "Time/Fbk/Rate/Bits";
        //                               "------------------" <-- Size Guide
    }
    
private:
    const int length = HEM_LOFI_PCM_BUFFER_SIZE;

    int16_t pcm[HEM_LOFI_PCM_BUFFER_SIZE];
    bool play = 0; //play always on unless gated on Digital 1
    uint16_t head = 0; // Location of read/play head
    uint16_t head_w = 0; // Location of write/record head
    int8_t dt_pct = 50; //delaytime as percentage of delayline buffer
    int8_t feedback = 50;
    int8_t fdbk_g = feedback;
    int8_t countdown = HEM_LOFI_PCM_SPEED;
    uint8_t rate = HEM_LOFI_PCM_SPEED;
    uint8_t rate_mod = rate;
    int depth = 1; // bit reduction depth aka bitcrush
    uint8_t cursor; //for gui
    
    void DrawWaveform() {
        int inc = rate_mod/2 + 1;
        int pos = head - (inc * 31) - random(1,3); // Try to center the head
        if (pos < 0) pos += length;
        for (int i = 0; i < 64; i++)
        {
            int height = Proportion(pcm[pos], CLIPLIMIT, 16);
            gfxLine(i, 46, i, 46+height);

            pos += inc;
            if (pos >= length) pos -= length;
        }
    }
    
    void DrawSelector()
    {
        if (cursor < 2) {
            for (int param = 0; param < 2; param++) {
                gfxIcon(31 * param, 15, param ? GAUGE_ICON : CLOCK_ICON );
            }
            gfxPrint(4 + pad(100, dt_pct), 15, dt_pct);
            gfxPrint(36 + pad(1000, fdbk_g), 15, fdbk_g);
            gfxCursor(0 + (31 * cursor), 23, 30);
        } else {
            gfxIcon(0, 15, WAVEFORM_ICON);
            gfxIcon(8, 15, BURST_ICON);
            gfxIcon(22, 15, LEFT_RIGHT_ICON);
            gfxPrint(30, 15, rate_mod);
            gfxIcon(42, 15, UP_DOWN_ICON);
            gfxPrint(50, 15, depth);
            gfxCursor(30 + (cursor-2)*20, 23, 14);
        }
    }
    
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to LoFiPCM,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
LoFiPCM LoFiPCM_instance[2];

void LoFiPCM_Start(bool hemisphere) {
    LoFiPCM_instance[hemisphere].BaseStart(hemisphere);
}

void LoFiPCM_Controller(bool hemisphere, bool forwarding) {
    LoFiPCM_instance[hemisphere].BaseController(forwarding);
}

void LoFiPCM_View(bool hemisphere) {
    LoFiPCM_instance[hemisphere].BaseView();
}

void LoFiPCM_OnButtonPress(bool hemisphere) {
    LoFiPCM_instance[hemisphere].OnButtonPress();
}

void LoFiPCM_OnEncoderMove(bool hemisphere, int direction) {
    LoFiPCM_instance[hemisphere].OnEncoderMove(direction);
}

void LoFiPCM_ToggleHelpScreen(bool hemisphere) {
    LoFiPCM_instance[hemisphere].HelpScreen();
}

uint32_t LoFiPCM_OnDataRequest(bool hemisphere) {
    return LoFiPCM_instance[hemisphere].OnDataRequest();
}

void LoFiPCM_OnDataReceive(bool hemisphere, uint32_t data) {
    LoFiPCM_instance[hemisphere].OnDataReceive(data);
}
