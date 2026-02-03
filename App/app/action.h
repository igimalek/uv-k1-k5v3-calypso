  


#ifndef APP_ACTION_H
#define APP_ACTION_H

#include "driver/keyboard.h"


void ACTION_Power(void);          
void ACTION_Monitor(void);        
void ACTION_Scan(bool bRestart);   

#ifdef ENABLE_VOX
void ACTION_Vox(void);            
#endif

#ifdef ENABLE_FMRADIO
void ACTION_FM(void);             
#endif

void ACTION_SwitchDemodul(void);   

#ifdef ENABLE_BLMIN_TMP_OFF
void ACTION_BlminTmpOff(void);    
#endif

  
#ifdef ENABLE_FEAT_F4HWN
void ACTION_RxMode(void);          
void ACTION_MainOnly(void);        
void ACTION_Ptt(void);             
void ACTION_Wn(void);              
void ACTION_BackLightOnDemand(void);   
void ACTION_BackLight(void);       
void ACTION_Mute(void);            
#endif

  
void ACTION_Handle(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);

#endif
