/* ========================================
 *
 * Copyright BACKYARD BRAINS, 2017
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF BACKYARD BRAINS.
 *
 * ========================================
*/
#include "project.h"
#include "StimulusGenerator.h"

//connection variables

int connectionStatus = 0;

CYBLE_CONN_HANDLE_T connectionHandle;

//length of blink during unconnected indicator
const int STATUS_BLINK_WIDTH = 100;

//stimulus generator struct
struct StimulusGenerator generator;

//sleep timer variables
const int SLEEP_TIMER_READY = 30;//sec
const int SLEEP_TIMER_ACTIVE = 120;//sec

int sleepCountdown;

uint8 gotoSleep;

//bluetooth communications handler
void StackHandler(uint32 eventCode, void* eventParam) {
    
    CYBLE_GATTS_WRITE_REQ_PARAM_T *wrReq;
    
    switch(eventCode) {
        case CYBLE_EVT_STACK_ON:
        
        case CYBLE_EVT_GAP_DEVICE_DISCONNECTED:
        
            CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST);
            
            //set connection bool to false
            connectionStatus = 0;
            
            //blink left and right led to indicate disconnect
            LeftLED_PWM_WriteCompare(STATUS_BLINK_WIDTH);
            
            RightLED_PWM_WriteCompare(STATUS_BLINK_WIDTH);
            
            //turn off green connection indicator light
            DurationTimer_WriteCompare(0);
            
            //start sleep countdown
            sleepCountdown = SLEEP_TIMER_READY;
            
            break;
            
        case CYBLE_EVT_GATT_CONNECT_IND:
            
            connectionHandle = *(CYBLE_CONN_HANDLE_T*)eventParam;
            
            //set connection bool to true
            connectionStatus = 1;
            
            //turn off left and right LEDs
            LeftLED_PWM_WriteCompare(0);
            
            RightLED_PWM_WriteCompare(0);
            
            
            //turn on green light to indicate connection
            DurationTimer_WriteCompare(1000);
            
            //start sleep countdown
            sleepCountdown = SLEEP_TIMER_ACTIVE;
            
            break;
            
        case CYBLE_EVT_GATTS_WRITE_REQ:
            
            wrReq = (CYBLE_GATTS_WRITE_REQ_PARAM_T*)eventParam;
            
            //handle left stimulus input, simply sets true whevenever the property is written to
            if (wrReq->handleValPair.attrHandle == CYBLE_ROBOROACH_STIMLEFT_CHAR_HANDLE) {
                
                CyBle_GattsWriteAttributeValue(&wrReq->handleValPair, 0, &connectionHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
                
                StimulusGenerator_StartStimulus(&generator, Left);
                
            }
            //handle right stimulus input
            if (wrReq->handleValPair.attrHandle == CYBLE_ROBOROACH_STIMRIGHT_CHAR_HANDLE) {
                
                CyBle_GattsWriteAttributeValue(&wrReq->handleValPair, 0, &connectionHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
                
                StimulusGenerator_StartStimulus(&generator, Right);
                
            }
            //handle duration input
            if (wrReq->handleValPair.attrHandle == CYBLE_ROBOROACH_DURATION_CHAR_HANDLE) {
                
                CyBle_GattsWriteAttributeValue(&wrReq->handleValPair, 0, &connectionHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
                
                StimulusGenerator_SetPulseDuration( &generator, wrReq->handleValPair.value.val[0]);
                
            }
            //handle frequency input
            if (wrReq->handleValPair.attrHandle == CYBLE_ROBOROACH_STIMFREQ_CHAR_HANDLE) {
                
                CyBle_GattsWriteAttributeValue(&wrReq->handleValPair, 0, &connectionHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
                
                StimulusGenerator_SetFrequency(&generator, wrReq->handleValPair.value.val[0]);
                
            }
            //handle pulse width input
            if (wrReq->handleValPair.attrHandle == CYBLE_ROBOROACH_STIMPULSE_CHAR_HANDLE) {
                
                CyBle_GattsWriteAttributeValue(&wrReq->handleValPair, 0, &connectionHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
                
                StimulusGenerator_SetPulseWidth(&generator, wrReq->handleValPair.value.val[0]); 
                
            }
            //handle gain input
            if (wrReq->handleValPair.attrHandle == CYBLE_ROBOROACH_GAIN_CHAR_HANDLE) {
                
                CyBle_GattsWriteAttributeValue(&wrReq->handleValPair, 0, &connectionHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
                
                StimulusGenerator_SetPulseGain(&generator, wrReq->handleValPair.value.val[0]); 
                
            }
            //handle random mode input
            if (wrReq->handleValPair.attrHandle == CYBLE_ROBOROACH_RANDOMMODE_CHAR_HANDLE) {
                
                CyBle_GattsWriteAttributeValue(&wrReq->handleValPair, 0, &connectionHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
                
                StimulusGenerator_SetRandomMode(&generator, wrReq->handleValPair.value.val[0]); 
                
            }
            
            CyBle_GattsWriteRsp(connectionHandle);
            sleepCountdown = SLEEP_TIMER_ACTIVE;
            
            break;
            
        default:
            
            break;
    }
}

//functions to put all components to sleep and wake them up
void SleepComponents() {
    PulseTimer_Stop();
    DurationTimer_Stop();
    LeftLED_PWM_Stop();
    RightLED_PWM_Stop();
    DAC_Stop();   
    CyBle_Stop();
}

void WakeComponents(){
    PulseTimer_Start();
    DurationTimer_Start();
    LeftLED_PWM_Start();
    RightLED_PWM_Start();
    DAC_Start();
    CyBle_Start(StackHandler);
}

//interrupt serive routines for sleep timer
CY_ISR(Wake_interrupt) {
    
    //clear wakeup interrupts
    Button_ClearInterrupt();
    
    //reset the sleep timer
    sleepCountdown = SLEEP_TIMER_READY;
    
    //if system was asleep, wake up components
    if (gotoSleep) {   
        
        WakeComponents();
        
        //clear sleep signal
        gotoSleep = 0;
    }
}

CY_ISR(Sleep_interrupt) {   
   //clear sleep interrupts
   CySysWdtClearInterrupt(CY_SYS_WDT_COUNTER0_INT);

   Sleep_ClearPending();

   //decrement sleep countdown, ~1 time per second
   sleepCountdown--;

   if(sleepCountdown == 0) {
        //set sleep bool
        gotoSleep = 1;
   }  
}

int main(void)
{
    CyGlobalIntEnable; /* Enable global interrupts. */
    
    
    //initialize sleep/wake interrupts
    Wake_StartEx(Wake_interrupt);
    Sleep_StartEx(Sleep_interrupt);
    gotoSleep = 0;
    
    //initalize BLE
    CyBle_Start(StackHandler);
    
    //start LED PWMs
    LeftLED_PWM_Start();
    
    RightLED_PWM_Start();
    
    //start RTC
    Clock_Start();
    
    //initalize parameters to defaults
    StimulusGenerator_Initialize(&generator);
    

    for(;;)
    {
        
        //process bluetooth communications
        CyBle_ProcessEvents();
        
        //update the currently active stimulus, if any
        StimulusGenerator_UpdateStimulus(&generator);
        
        //go to sleep if idle
        if(gotoSleep) {
            SleepComponents();
            CySysPmDeepSleep();  
        }
    }
}

/* [] END OF FILE */
