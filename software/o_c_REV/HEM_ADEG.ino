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


//modded for trapezoid envelope
//needs a setting to ignore the clock? so maybe if rt time is >0 it will disregard clock

#define HEM_ADEG_MAX_VALUE 255
#define HEM_ADEG_MAX_TICKS 33333 //added2

class ADEG : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "AD EG";
    }

    void Start() {
        signal = 0;
        phase = 0;
        attack = 50;
        decay = 50;
        hold = 25000;
        retrig = 0; //cycle is off if retrig is 0
    }

    void Controller() {
        if (Clock(0) && retrig <= 0) {
            // Trigger the envelope
            phase = 1; // Return to attack phase
            effective_attack = attack;
            effective_decay = decay;
            
        } else if (Clock(1)) {
            // Trigger the envelope in reverse
            phase = 1;
            effective_attack = decay;
            effective_decay = attack;
            
        } else if (phase == 0 && retrig > 0){ //wait to retrig if no clock signal  phase == 0 && 
                    retrig --;
                    if (retrig <= 0) {
                      phase = 1;
                      retrig = rt_time * 1000; //reset retrig count
                      effective_attack = attack;
                      effective_decay = decay;
                    }
        }

        if (phase > 0) {
            simfloat target;
            if (phase == 1) target = int2simfloat(HEMISPHERE_MAX_CV); // Rise to max for attack
            if (phase == 3) target = 0; // Fall to zero for decay

            //if (signal != target) { // Logarhythm fix 8/2020
                int segment = phase == 1
                    ? effective_attack + Proportion(DetentedIn(0), HEMISPHERE_MAX_CV, HEM_ADEG_MAX_VALUE)
                    : effective_decay + Proportion(DetentedIn(1), HEMISPHERE_MAX_CV, HEM_ADEG_MAX_VALUE);
                segment = constrain(segment, 0, HEM_ADEG_MAX_VALUE);
                simfloat remaining = target - signal;

                // The number of ticks it would take to get from 0 to HEMISPHERE_MAX_CV
                int max_change = Proportion(segment, HEM_ADEG_MAX_VALUE, HEM_ADEG_MAX_TICKS);

                // The number of ticks it would take to move the remaining amount at max_change
                int ticks_to_remaining = Proportion(simfloat2int(remaining), HEMISPHERE_MAX_CV, max_change);
                if (ticks_to_remaining < 0) ticks_to_remaining = -ticks_to_remaining;

                simfloat delta;
                if (ticks_to_remaining <= 0) {
                    delta = remaining;
                } else {
                    delta = remaining / ticks_to_remaining;
                }
                signal += delta;

                if (simfloat2int(signal) >= HEMISPHERE_MAX_CV && phase == 1) phase = 2; //go on to hold phase

                if (phase == 2) {
                  hold --;              
                if (hold <= 0) phase = 3; 
                }
                
                // Check for EOC
                if (simfloat2int(signal) <= 0 && phase == 3) {
                    ClockOut(1);
                    phase = 0;
                    hold = on_time * 500; //reset hold count
                    
                }
                
                
            //}
            Out(0, simfloat2int(signal));
        }
    }

    void View() {
        gfxHeader(applet_name());
        DrawIndicator();
        DrawSelector();
    }

    void OnButtonPress() {
        if (++cursor > 3) cursor = 0;
        ResetCursor();
    }

    void OnEncoderMove(int direction) {
        if (cursor == 0) {
            attack = constrain(attack += direction, 0, HEM_ADEG_MAX_VALUE);
            last_ms_value = Proportion(attack, HEM_ADEG_MAX_VALUE, HEM_ADEG_MAX_TICKS) / 17;
        }
        else if (cursor == 1){
            decay = constrain(decay += direction, 0, HEM_ADEG_MAX_VALUE);
            last_ms_value = Proportion(decay, HEM_ADEG_MAX_VALUE, HEM_ADEG_MAX_TICKS) / 17;
        }
        else if (cursor == 2){
            //hold 2.5 secs
            on_time = constrain(on_time += direction, 0, 99); //2.5secs will have to be 50,000 ticks
            hold = on_time * 500;
            last_ms_value = on_time * 25;
        }
        else if (cursor == 3){
            //off time 5 secs
            rt_time = constrain(rt_time += direction, 0, 99); //5 secs will have to be 100,000 ticks
            retrig = rt_time * 1000;
            last_ms_value = rt_time * 50;

        }
        
        last_change_ticks = OC::CORE::ticks;
    }
        
    uint32_t OnDataRequest() {
        uint32_t data = 0;
        Pack(data, PackLocation {0,8}, attack);
        Pack(data, PackLocation {8,8}, decay);
        return data;
    }

    void OnDataReceive(uint32_t data) {
        attack = Unpack(data, PackLocation {0,8});
        decay = Unpack(data, PackLocation {8,8});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Trg 2=Trg Revers";
        help[HEMISPHERE_HELP_CVS]      = "1=A mod 2=D mod";
        help[HEMISPHERE_HELP_OUTS]     = "A=Output B=EOC";
        help[HEMISPHERE_HELP_ENCODER]  = "Attack/Decay";
        //                               "------------------" <-- Size Guide
    }
    
private:
    simfloat signal; // Current signal level for each channel
    int phase; // 0=Not running 1=Attack 2=Decay
    int cursor; // 0 = Attack, 1 = Decay
    int last_ms_value;
    int last_change_ticks;
    int effective_attack; // Attack and decay for this particular triggering
    int effective_decay;  // of the EG, so that it can be triggered in reverse!

    // Settings
    int attack; // Time to reach signal level if signal < 5V
    int decay; // Time to reach signal level if signal > 0V
    int hold; //counter for hold of trapezoid envelope
    int retrig; //counter for off time of trapezoid 
    int on_time = 50; //on_time is the controller value of hold counter;
    int rt_time = 0; //time before retriggering, 0 = no retrig
    
    void DrawIndicator() {
        int a_x = Proportion(attack, HEM_ADEG_MAX_VALUE, 31);
        int d_x = a_x + Proportion(decay, HEM_ADEG_MAX_VALUE, 31);

        if (d_x > 0) { // Stretch to use the whole viewport
            a_x = Proportion(62, d_x, a_x);
            d_x = Proportion(62, d_x, d_x);
        }

        gfxLine(0, 62, a_x, 33, cursor == 1);
        gfxLine(a_x, 33, d_x, 62, cursor == 0);

        // Output indicators
        gfxRect(1, 15, ProportionCV(ViewOut(0), 62), 6);

        // Change indicator, if necessary
        if (OC::CORE::ticks - last_change_ticks < 20000) {
            gfxPrint(15, 43, last_ms_value);
            gfxPrint("ms");
        }
    }
    
    void DrawSelector() // this is for the hold time and the off time
    {
        for (int param = 0; param < 2; param++)
        { 
            if(param == 0 || param == 1) gfxPrint(31 * param, 22, param ? "Rt: " : "On: ");
            //if(param == 2 || param == 3) gfxPrint(31 * (param - 2), 30, (param - 2) ? "f2: " : "Dm: ");

            gfxPrint(16, 22, on_time);
            gfxPrint(48, 22, rt_time);
            if (param == cursor - 2){
              gfxCursor(31 * (param % 2), (15*(param/2))+30, 30);
            }
        }
    }
    
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to ADEG,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
ADEG ADEG_instance[2];

void ADEG_Start(bool hemisphere) {ADEG_instance[hemisphere].BaseStart(hemisphere);}
void ADEG_Controller(bool hemisphere, bool forwarding) {ADEG_instance[hemisphere].BaseController(forwarding);}
void ADEG_View(bool hemisphere) {ADEG_instance[hemisphere].BaseView();}
void ADEG_OnButtonPress(bool hemisphere) {ADEG_instance[hemisphere].OnButtonPress();}
void ADEG_OnEncoderMove(bool hemisphere, int direction) {ADEG_instance[hemisphere].OnEncoderMove(direction);}
void ADEG_ToggleHelpScreen(bool hemisphere) {ADEG_instance[hemisphere].HelpScreen();}
uint32_t ADEG_OnDataRequest(bool hemisphere) {return ADEG_instance[hemisphere].OnDataRequest();}
void ADEG_OnDataReceive(bool hemisphere, uint32_t data) {ADEG_instance[hemisphere].OnDataReceive(data);}
