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

#include "u_law.h"

#define HEM_LOFI_PCM_BUFFER_SIZE 2048
#define HEM_LOFI_PCM_SPEED 8
#define LOFI_PCM2CV(S) ((int32_t)S << 8) - 32512 //32767 is 128 << 8 32512 is 127 << 8 // 0-126 is neg, 127 is 0, 128-254 is pos

class LoFiPCM : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "LoFi Echo";
    }

    void Start() {
        countdown = HEM_LOFI_PCM_SPEED;
        for (int i = 0; i < HEM_LOFI_PCM_BUFFER_SIZE; i++) pcm[i] = 127; //char is unsigned in teensy (0-255)?
        selected = 1; //for gui
    }

    void Controller() {
        play = !Gate(0); // Continuously play unless gated
        gated_record = Gate(1);

        countdown--;
        if (countdown == 0) {
            head++;
            if (head >= length) {
                head = 0;               
                ClockOut(1);
            }
            int dt = delaytime_pct * length / 100; //convert delaytime to length in samples 
            int writehead = (head+length + dt) % length; //have to add the extra length to keep modulo positive in case delaytime is neg
            //int32_t tapeout = LOFI_PCM2CV(pcm[head]); // get the char out from the array and convert back to cv (de-offset)
            int16_t tapeout = ulaw_decode_table[(uint8_t)pcm[head]]; // get the char out from the array and convert back to cv (de-offset)

            //int32_t feedbackmix = constrain(((tapeout * feedback / 100  + In(0)) + 32640), locliplimit, cliplimit) >> 8; //add to the feedback, offset and bitshift down; 32640 to fix rounding error
            int16_t feedbackmix = ulaw_encode(constrain(((tapeout * feedback / 100  + In(0))), locliplimit, cliplimit)); //use the ulaw_encode function instead

            pcm[writehead] = (char)feedbackmix;
            
            //int32_t s = LOFI_PCM2CV(pcm[head]);
            int16_t s = ulaw_decode_table[(uint8_t)pcm[head]];
            int SOS = In(1); // Sound-on-sound
            int live = Proportion(SOS, HEMISPHERE_MAX_CV, In(0)); //max_cv is 7680 scales vol. of live 
            int loop = play ? Proportion(HEMISPHERE_MAX_CV - SOS, HEMISPHERE_MAX_CV, s) : 0;
            Out(0, live + loop);
            countdown = HEM_LOFI_PCM_SPEED;
        }
    }

    void View() {
        gfxHeader(applet_name());
        DrawSelector();
        DrawWaveform();
    }

    void OnButtonPress() {
        selected = 1 - selected;
        ResetCursor();
    }

    void OnEncoderMove(int direction) {
        if (selected == 0) delaytime_pct = constrain(delaytime_pct += direction, 0, 99);
        if (selected == 1) feedback = constrain(feedback += direction, 0, 99);

        //amp_offset_cv = Proportion(amp_offset_pct, 100, HEMISPHERE_MAX_CV);
        //p[cursor] = constrain(p[cursor] += direction, 0, 100);

    
    }

    uint32_t OnDataRequest() {
        uint32_t data = 0;
        return data;
    }

    void OnDataReceive(uint32_t data) {
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "Gate 1=Pause 2=Rec";
        help[HEMISPHERE_HELP_CVS]      = "1=Audio 2=SOS";
        help[HEMISPHERE_HELP_OUTS]     = "A=Audio B=EOC Trg";
        help[HEMISPHERE_HELP_ENCODER]  = "T=End Pt P=Rec";
        //                               "------------------" <-- Size Guide
    }
    
private:
    char pcm[HEM_LOFI_PCM_BUFFER_SIZE];
    bool record = 0; // Record always on
    bool gated_record = 0; // Record gated via digital in
    bool play = 0; //play always on
    int head = 0; // Locatioon of play/record head
    int delaytime_pct = 50; //delaytime as percentage of delayline buffer
    int feedback = 50;
    int countdown = HEM_LOFI_PCM_SPEED;
    int length = HEM_LOFI_PCM_BUFFER_SIZE;
    int32_t cliplimit = 65024;
    int32_t locliplimit = 0;
/*    const int16_t ulaw_decode_table[256] = {
       4,    12,    20,    28,    36,    44,    52,    60,    68,    76,
      84,    92,   100,   108,   116,   124,   136,   152,   168,   184,
     200,   216,   232,   248,   264,   280,   296,   312,   328,   344,
     360,   376,   400,   432,   464,   496,   528,   560,   592,   624,
     656,   688,   720,   752,   784,   816,   848,   880,   928,   992,
    1056,  1120,  1184,  1248,  1312,  1376,  1440,  1504,  1568,  1632,
    1696,  1760,  1824,  1888,  1984,  2112,  2240,  2368,  2496,  2624,
    2752,  2880,  3008,  3136,  3264,  3392,  3520,  3648,  3776,  3904,
    4096,  4352,  4608,  4864,  5120,  5376,  5632,  5888,  6144,  6400,
    6656,  6912,  7168,  7424,  7680,  7936,  8320,  8832,  9344,  9856,
   10368, 10880, 11392, 11904, 12416, 12928, 13440, 13952, 14464, 14976,
   15488, 16000, 16768, 17792, 18816, 19840, 20864, 21888, 22912, 23936,
   24960, 25984, 27008, 28032, 29056, 30080, 31104, 32128,    -4,   -12,
     -20,   -28,   -36,   -44,   -52,   -60,   -68,   -76,   -84,   -92,
    -100,  -108,  -116,  -124,  -136,  -152,  -168,  -184,  -200,  -216,
    -232,  -248,  -264,  -280,  -296,  -312,  -328,  -344,  -360,  -376,
    -400,  -432,  -464,  -496,  -528,  -560,  -592,  -624,  -656,  -688,
    -720,  -752,  -784,  -816,  -848,  -880,  -928,  -992, -1056, -1120,
   -1184, -1248, -1312, -1376, -1440, -1504, -1568, -1632, -1696, -1760,
   -1824, -1888, -1984, -2112, -2240, -2368, -2496, -2624, -2752, -2880,
   -3008, -3136, -3264, -3392, -3520, -3648, -3776, -3904, -4096, -4352,
   -4608, -4864, -5120, -5376, -5632, -5888, -6144, -6400, -6656, -6912,
   -7168, -7424, -7680, -7936, -8320, -8832, -9344, -9856,-10368,-10880,
  -11392,-11904,-12416,-12928,-13440,-13952,-14464,-14976,-15488,-16000,
  -16768,-17792,-18816,-19840,-20864,-21888,-22912,-23936,-24960,-25984,
  -27008,-28032,-29056,-30080,-31104,-32128
  };
*/  
    int selected; //for gui
     
    
    void DrawWaveform() {
        int inc = HEM_LOFI_PCM_BUFFER_SIZE / 256;
        int disp[32];
        int high = 1;
        int pos = head - (inc * 15) - random(1,3); // Try to center the head
        if (head < 0) head += length;
        for (int i = 0; i < 32; i++)
        {
            int v = (int)pcm[pos] - 0; //maybe change -127 to -0 because of ulaw
            if (v < 0) v = 0;
            if (v > high) high = v;
            pos += inc;
            if (pos >= HEM_LOFI_PCM_BUFFER_SIZE) pos -= length;
            disp[i] = v;
        }
        
        for (int x = 0; x < 32; x++)
        {
            int height = Proportion(disp[x], high, 30);
            int margin = (32 - height) / 2;
            gfxLine(x * 2, 30 + margin, x * 2, height + 30 + margin);
        }
    }
    

    
    void DrawSelector()
    {
        for (int param = 0; param < 2; param++)
        {
            gfxPrint(31 * param, 15, param ? "Fb: " : "Ln: ");
            gfxPrint(16, 15, delaytime_pct);
            gfxPrint(48, 15, feedback);
            if (param == selected) gfxCursor(0 + (31 * param), 23, 30);
        }
    }
    
    uint8_t ulaw_encode(int16_t audio)
    {
      uint32_t mag, neg;
      // http://en.wikipedia.org/wiki/G.711
      if (audio >= 0) {
        mag = audio;
        neg = 0;
      } else {
        mag = audio * -1;
        neg = 0x80;
       }
      mag += 128;
      if (mag > 0x7FFF) mag = 0x7FFF;
      if (mag >= 0x4000) return neg | 0x70 | ((mag >> 10) & 0x0F);  // 01wx yz00 0000 0000
      if (mag >= 0x2000) return neg | 0x60 | ((mag >> 9) & 0x0F);   // 001w xyz0 0000 0000
      if (mag >= 0x1000) return neg | 0x50 | ((mag >> 8) & 0x0F);   // 0001 wxyz 0000 0000
      if (mag >= 0x0800) return neg | 0x40 | ((mag >> 7) & 0x0F);   // 0000 1wxy z000 0000
      if (mag >= 0x0400) return neg | 0x30 | ((mag >> 6) & 0x0F);   // 0000 01wx yz00 0000
      if (mag >= 0x0200) return neg | 0x20 | ((mag >> 5) & 0x0F);   // 0000 001w xyz0 0000
      if (mag >= 0x0100) return neg | 0x10 | ((mag >> 4) & 0x0F);   // 0000 0001 wxyz 0000
      else               return neg | 0x00 | ((mag >> 3) & 0x0F);   // 0000 0000 1wxy z000
   }
    
 /*   void DrawStop(int x, int y) {
        if (record || play || gated_record) gfxFrame(x, y, 11, 11);
        else gfxRect(x, y, 11, 11);
    }
    
    void DrawPlay(int x, int y) {
        if (play) {
            for (int i = 0; i < 11; i += 2)
            {
                gfxLine(x + i, y + i/2, x + i, y + 10 - i/2);
                gfxLine(x + i + 1, y + i/2, x + i + 1, y + 10 - i/2);
            }
        } else {
            gfxLine(x, y, x, y + 10);
            gfxLine(x, y, x + 10, y + 5);
            gfxLine(x, y + 10, x + 10, y + 5);
        }
    }
    void DrawRecord(int x, int y) {
        gfxCircle(x + 5, y + 5, 5);
        if (record || gated_record) {
            for (int r = 1; r < 5; r++)
            {
                gfxCircle(x + 5, y + 5, r);
            }
        }
    }
*/    
    
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
