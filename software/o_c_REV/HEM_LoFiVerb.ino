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

#define HEM_LOFI_VERB_BUFFER_SIZE 307
#define HEM_LOFI_VERB_ALLPASS_SIZE 106

#define HEM_LOFI_VERB_SPEED 2 //cant be lower than 2 for memory reasons unless lofi echo is removed

#define LOFI_PCM2CV(S) ((int32_t)S << 8) - 32512 //32767 is 128 << 8 32512 is 127 << 8 // 0-126 is neg, 127 is 0, 128-254 is pos

class LoFiVerb : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "LoFi Verb";
    }

    void Start() {
        countdown = HEM_LOFI_VERB_SPEED;
        for (int i = 0; i < HEM_LOFI_VERB_BUFFER_SIZE; i++) pcm[i] = 127; //char is unsigned in teensy (0-255)?   
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < HEM_LOFI_VERB_ALLPASS_SIZE; j++) allpass_pcm[i][j] = 127;
        }
        
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
                //ClockOut(1);
            }
            
            ap_head++;
            if (ap_head >= ap_length) {
                ap_head = 0;               
               
            }

            for (int i = 0; i < 8; i++){ //for each of the 8 multitap heads; 
                int dt = multitap_heads[i] / HEM_LOFI_VERB_SPEED; //convert delaytime to length in samples 
                int writehead = (head+length + dt) % length; //have to add the extra length to keep modulo positive in case delaytime is neg   
                //int32_t tapeout = LOFI_PCM2CV(pcm[head]); // get the char out from the array and convert back to cv (de-offset);
                int16_t tapeout = ulaw_decode_table[(uint8_t)pcm[head]]; // get the char out from the array and convert back to cv (de-offset)

                if (dampen_on != 0) { //lowpass filter at cutoff freq                                                           
                    int w = 2*8300; //2 x samplerate
                    cutoff *= 2 * 22 / 7; // cutoff * 2 * pi;
                    int norm = 1/(cutoff + w);
                    int b1 =  (w - cutoff) * norm;
                    int a0 = cutoff * norm;
                    int a1 = a0;
                    //int32_t last_out = LOFI_PCM2CV(pcm[writehead - 1]);
                    int16_t last_out = ulaw_decode_table[(uint8_t)pcm[writehead - 1]];
                    int32_t damp_out = tapeout * a0 + dampen[i] * a1 + last_out * b1; 
                    dampen[i] = tapeout; //dampen [i] is last input
                    tapeout = damp_out;
                    };    
                    
                //int32_t feedbackmix = constrain(((tapeout * feedback / 100  + In(0)) + 32640), locliplimit, cliplimit) >> 8; //add to the feedback, offset and bitshift down
                int16_t feedbackmix = ulaw_encode(constrain(((tapeout * feedback / 100  + In(0))), locliplimit, cliplimit)); //use the ulaw_encode function instead

                pcm[writehead] = (char)feedbackmix;
            }

            //char ap = pcm[head]; //8 bit char of result of multitap 0 to 254            
            //int32_t ap_int = LOFI_PCM2CV(pcm[head]); //convert to signed full scale
            int16_t ap_int = ulaw_decode_table[(uint8_t)pcm[head]];
            int32_t mix = (ap_int ) + In(0); // mix 1/8 signal of comb with input;
            
            if (allpass==1){ 
                for (int i = 0; i < 4; i++){ //diffusors in series -- all done in 8 bit signed int
                    int32_t dry = mix;
                    int dt = allpass_delay_times[i] / HEM_LOFI_VERB_SPEED; //delay time
                    int writehead = (ap_head + ap_length + dt) % ap_length; //add delay time to get write location
                    //int32_t tapeout = LOFI_PCM2CV(allpass_pcm[i][ap_head]);
                    int16_t tapeout = ulaw_decode_table[(uint8_t)pcm[ap_head]]; 

                
                    //int32_t feedbackmix = constrain(((tapeout * feedback2 / 100  + dry) + 32640),locliplimit, cliplimit) >> 8; //add to the feedback (50%), clip at 127 //buffer[bufidx] = input + (bufout*feedback);
                    int16_t feedbackmix = ulaw_encode(constrain(((tapeout * feedback2 / 100  + dry)), locliplimit, cliplimit)); //use the ulaw_encode function instead

                    allpass_pcm[i][writehead] = (char)feedbackmix; 
                    mix =  (tapeout - ((tapeout * feedback2/100) + dry) * feedback2/100 ); //freeverb 3: _fv3_float_t output = bufout - buffer[bufidx] * feedback;
                       

                
                    //mix = tapeout + dry*(-1);//orig. freeverb
                
                }                 
            }
            
            //char ap = (char)ap_int;
            //uint32_t s = LOFI_PCM2CV(ap); //convert back to CV scale
            uint32_t s = mix;

            //uint32_t s = LOFI_PCM2CV(pcm[head]); 

            int SOS = In(1); // Sound-on-sound
            int live = Proportion(SOS, HEMISPHERE_MAX_CV, In(0)); //max_cv is 7680 scales vol. of live 
            int loop = play ? Proportion(HEMISPHERE_MAX_CV - SOS, HEMISPHERE_MAX_CV, s) : 0;
            Out(0, live + loop);
            countdown = HEM_LOFI_VERB_SPEED;
            
        }
    }

    void View() {
        gfxHeader(applet_name());
        DrawSelector();
        //DrawWaveform();
    }

    void OnButtonPress() {
        if (++selected > 3) selected = 0;
        ResetCursor();
    }

    void OnEncoderMove(int direction) {
        if (selected == 0) allpass =  constrain(allpass += direction, 0, 1);
        if (selected == 1) feedback = constrain(feedback += direction, 0, 99);
        if (selected == 2) dampen_on = constrain(dampen_on += direction, 0, 1);
        if (selected == 3) feedback2 = constrain(feedback2 += direction, 0, 99);

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
    char pcm[HEM_LOFI_VERB_BUFFER_SIZE];
    uint16_t multitap_heads[8] = {438,613,565,538,484,514,450,422}; //adapted for 16.7khz
    uint16_t allpass_delay_times[4] = {85,129,167,210}; //adapted for 16.7khz
    char allpass_pcm[4][HEM_LOFI_VERB_ALLPASS_SIZE]; //4 buffers of 105 samples each
    int32_t dampen[8] = {0,0,0,0,0,0,0,0}; //stores the last value for dampening/averaging    
    bool record = 0; // Record always on
    bool gated_record = 0; // Record gated via digital in
    bool play = 0; //play always on
    int head = 0; // Location of delay head
    int ap_head = 0; // Location of allpass head
    bool allpass = 1; //allpass on or off
    bool dampen_on = 0; //dampen on or off
    int8_t feedback = 80;
    int8_t feedback2 = 50;
    int countdown = HEM_LOFI_VERB_SPEED;
    int length = HEM_LOFI_VERB_BUFFER_SIZE;
    int ap_length = HEM_LOFI_VERB_ALLPASS_SIZE;
    int32_t cliplimit = 65024;
    int32_t locliplimit = 0;
    int selected; //for gui
    int cutoff = 400;
    
    void DrawSelector()
    {
        for (int param = 0; param < 4; param++)
        { 
            if(param == 0 || param == 1) gfxPrint(31 * param, 15, param ? "Fb: " : "Ap: ");
            if(param == 2 || param == 3) gfxPrint(31 * (param - 2), 30, (param - 2) ? "f2: " : "Dm: ");

            gfxPrint(16, 15, allpass);
            gfxPrint(48, 15, feedback);
            gfxPrint(16, 30, dampen_on);
            gfxPrint(48, 30, feedback2);
            if (param == selected){
              gfxCursor(31 * (param % 2), (15*(param/2))+23, 30);
            }
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
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to LoFiVerb,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
LoFiVerb LoFiVerb_instance[2];

void LoFiVerb_Start(bool hemisphere) {
    LoFiVerb_instance[hemisphere].BaseStart(hemisphere);
}

void LoFiVerb_Controller(bool hemisphere, bool forwarding) {
    LoFiVerb_instance[hemisphere].BaseController(forwarding);
}

void LoFiVerb_View(bool hemisphere) {
    LoFiVerb_instance[hemisphere].BaseView();
}

void LoFiVerb_OnButtonPress(bool hemisphere) {
    LoFiVerb_instance[hemisphere].OnButtonPress();
}

void LoFiVerb_OnEncoderMove(bool hemisphere, int direction) {
    LoFiVerb_instance[hemisphere].OnEncoderMove(direction);
}

void LoFiVerb_ToggleHelpScreen(bool hemisphere) {
    LoFiVerb_instance[hemisphere].HelpScreen();
}

uint32_t LoFiVerb_OnDataRequest(bool hemisphere) {
    return LoFiVerb_instance[hemisphere].OnDataRequest();
}

void LoFiVerb_OnDataReceive(bool hemisphere, uint32_t data) {
    LoFiVerb_instance[hemisphere].OnDataReceive(data);
}
