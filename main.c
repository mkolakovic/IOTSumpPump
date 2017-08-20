//*****************************************************************************
//  Copyright © 2017 Mirza Kolakovic.
//
//  This program is distributed under the terms of the GNU General Public License.
//
//  mkolakov@gmail.com
//
//  March 9 2017
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//*****************************************************************************

//*****************************************************************************
//
// Application Name     - IOT Sump_Pump
// Application Overview - The IOT Sump Pump application on the CC3200 sends emails
//                        using SMTP (Simple Mail Transfer Protocol) upon detecting
//                        GPIO13 switching High or Vbat <2.4V.
// Tool-chain           - CCS 7 & CC3200 SDK 1.2
//*****************************************************************************

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
// simplelink includes
#include "simplelink.h"
// driverlib includes
#include "adc.h"
#include "hw_memmap.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "interrupt.h"
#include "prcm.h"
#include "rom.h"
#include "rom_map.h"
#include "pin.h"
#include "utils.h"
#include "pinmux.h"
// email
#include "email.h"
// common interface includes
#include "uart_if.h"
#include "gpio.h"
#include "gpio_if.h"
#include "time.h"
#include "network_if.h"
#include "common.h"
// network/email settings
#include "config.h"

// ----------------------------------------------------------------------------
#define DELAY_1MS            8000
#define DELAY_1SEC           8000000
#define SLOW_CLK_FREQ        (32*1024)
#define SW_ON                1
#define SW_OFF               0
#define DEFAULT_WAKEUP       (8*60*60) // 8 hours
#define EMERGENCY_WAKEUP     (30*60)   // 30 minutes
#define NO_OF_ADC_SAMPLES    15
#define LOW_VOLTAGE          2700 // in mV = 2.7V
#define AP_CONNECT_ERROR     -1
#define EMAIL_SEND_ERROR     -2


#define SUMPPUMP_EMAIL_MSG  "SUMP PUMP WATER LEVEL CRITICAL !!!"
#define LOW_VBAT_EMAIL_MSG  "IOT SUMP PUMP BATTERY LOW !!!"
//*****************************************************************************
//                 GLOBAL VARIABLES
//*****************************************************************************
extern void (* const g_pfnVectors[])(void);

// AP Security Parameters
SlSecParams_t SecurityParams = {0};

//*****************************************************************************
//                LOCAL FUNCTION PROTOTYPES
//*****************************************************************************

static void BoardInit(void);

uint32_t ReadInput(void);

uint32_t VbatMon(void);

int32_t SetEmailParameters(void);

int32_t SendEmail(char* msg);

int32_t eMailErrorHadler(int32_t error, char * servermessage);

void Hibernate(void);

void CheckStatus(int32_t status);

void Flash_LED(unsigned char LED);

//****************************************************************************
//
// Main function
//
//****************************************************************************
int main()
{
    uint32_t sw3_Sump; // variable to store water level switch state,
                       // this happens to be SW3 on the Launch-pad
    uint32_t vbat;
    int32_t status;

    //
    // Initialize Board configurations
    //
    BoardInit();

    // LED = On
    GPIO_IF_LedOn(MCU_RED_LED_GPIO);

    // Initialize AP security params
    //
    SecurityParams.Key = (signed char *)SECURITY_KEY;
    SecurityParams.KeyLen = strlen(SECURITY_KEY);
    SecurityParams.Type = MY_SECURITY_TYPE;

    //
    // check battery voltage
    //
    DBG_PRINT("\n\n\n\r");
    DBG_PRINT("\t\t ***************  Checking battery voltage *******************\n\r");
    DBG_PRINT("\n\n\n\r");

    vbat = VbatMon();

    DBG_PRINT("\t\t vbat =  %d mV     \n\r", vbat);
    DBG_PRINT("\n\n\n\r");

    if(vbat < LOW_VOLTAGE)
    {
        DBG_PRINT("\n\n\n\r");
        DBG_PRINT("\t\t *********  Sending eMail - Battery LOW *****************\n\r");
        DBG_PRINT("\n\n\n\r");

        status = SendEmail(LOW_VBAT_EMAIL_MSG);
        CheckStatus(status);
    }

    //
    // Read PIN_04 = GPIO13 = SW3 '0'==OFF
    //
    DBG_PRINT("\n\n\n\r");
    DBG_PRINT("\t\t ***********  Reading water level sensor  ***********\n\r");
    DBG_PRINT("\n\n\n\r");
    //
    sw3_Sump = ReadInput();
    //
    DBG_PRINT("\t\t sw3_Sump =  %d        \n\r", sw3_Sump);
    DBG_PRINT("\n\n\n\r");

    if( sw3_Sump == SW_OFF )
    {
        Hibernate(); // GO TO SLEEP
    }

    //
    //  OTHERWISE....
    //
    DBG_PRINT("\n\n\n\r");
    DBG_PRINT("\t\t ***************  Sending eMail *****************\n\r");
    DBG_PRINT("\n\n\n\r");
    //
    //  Send EMAIL
    //
    status = SendEmail(SUMPPUMP_EMAIL_MSG);
    CheckStatus(status);

    //
    // Configure the HIB module RTC wake time
    //
    MAP_PRCMHibernateIntervalSet(EMERGENCY_WAKEUP * SLOW_CLK_FREQ);
    //
    // Enable the HIB RTC and disable GPIO (HIBERNATE MODE)
    //
    MAP_PRCMHibernateWakeupSourceDisable(PRCM_HIB_GPIO13);
    MAP_PRCMHibernateWakeupSourceEnable(PRCM_HIB_SLOW_CLK_CTR);

    //
    // Hibernate
    //

    Hibernate();

}  // end of Main

//*****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS                         
//*****************************************************************************

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void BoardInit(void)
{
    //
    // Set vector table base
    //
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);

    
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();

    //
    // Power on the corresponding GPIO port B for 9,10,11.
    // Set up the GPIO lines to mode 0 (GPIO)
    //
    PinMuxConfig();
    GPIO_IF_LedConfigure(LED1|LED2|LED3);
    GPIO_IF_LedOff(MCU_ALL_LED_IND);
    //
    // Configure the HIB module RTC wake time and GPIO
    //
    MAP_PRCMHibernateIntervalSet(DEFAULT_WAKEUP * SLOW_CLK_FREQ);
    MAP_PRCMHibernateWakeUpGPIOSelect(PRCM_HIB_GPIO13, PRCM_HIB_HIGH_LEVEL);
    //
    // Enable the HIB RTC and GPIO (HIBERNATE MODE)
    //
    MAP_PRCMHibernateWakeupSourceEnable(PRCM_HIB_GPIO13 | PRCM_HIB_SLOW_CLK_CTR);
    //
    // Set up UART
    // Initializing the Terminal.
    //
    InitTerm();
    //
    // print start up message to console
    //
    DBG_PRINT("\n\n\n\r");
    DBG_PRINT("\t\t ***************  Board Initialized*******************\n\r");
    DBG_PRINT("\n\n\n\r");
}



//*****************************************************************************
//
// Reading and De-bouncing WATER LEVEL SENSOR INPUT
//
// return 32-BIT UNSIGNED = input state, 0 == OFF
//
//*****************************************************************************
uint32_t ReadInput(void)
{
    uint32_t sw3;
    uint32_t i, sw3_debounced;

    sw3_debounced = 0;

    for(i=0; i<100; i++) // read 100 times, no false alarm wanted here
    {
        sw3 = MAP_GPIOPinRead(GPIOA1_BASE, GPIO_PIN_5); // read pin
        sw3 = (sw3 > 5) & 0x01; // shift value
        sw3_debounced += sw3; // remember the value
        MAP_UtilsDelay(10*DELAY_1MS); //delay before reading input again
    }

    // debounce, return '1' only if it was '1' every consecutive time
    if(sw3_debounced == i)
        sw3_debounced = 1;
    else
        sw3_debounced = 0;

    return sw3_debounced;
}

//*****************************************************************************
//
// Battery Voltage Monitor
//
// return parameter:  battery voltage in mV
//
//*****************************************************************************
uint32_t VbatMon(void)
{
    uint32_t uiIndex=0;
    uint32_t ulSample;
    uint32_t ulSum;
    uint32_t pulAdcSamples[NO_OF_ADC_SAMPLES+5];
    //
    // Pinmux for the selected ADC input pin
    //
    MAP_PinTypeADC(PIN_58,PIN_MODE_255);
    //
    // Configure ADC timer which is used to timestamp the ADC data samples
    //
    MAP_ADCTimerConfig(ADC_BASE,2^17);
    //
    // Enable ADC timer which is used to timestamp the ADC data samples
    //
    MAP_ADCTimerEnable(ADC_BASE);
    //
    // Enable ADC module
    //
    MAP_ADCEnable(ADC_BASE);
    //
    // Enable ADC channel
    //
    MAP_ADCChannelEnable(ADC_BASE, ADC_CH_1);
    //
    // read FIFO
    //
    while(uiIndex < NO_OF_ADC_SAMPLES+5) // will drop first 5 samples !!!
    {
        if(MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_1))
        {
            ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_1);
            pulAdcSamples[uiIndex++] = ulSample;
        }
    }

    MAP_ADCChannelDisable(ADC_BASE, ADC_CH_1);

    uiIndex = 0;
    ulSum = 0;

    //DBG_PRINT("\n\rADC data format:\n\r");
    //DBG_PRINT("\n\rbits[13:2] : ADC sample\n\r");
    //DBG_PRINT("\n\rbits[31:14]: Time stamp of ADC sample \n\r");
    //
    // Print out ADC samples
    //
    //  Full Scale at the pin is 1.467V and 2.934V on the supply
    //
    while(uiIndex < NO_OF_ADC_SAMPLES)
    {
        ulSum += ((((pulAdcSamples[5+uiIndex] >> 2 ) & 0x0FFF))*2934)/4096;
        uiIndex++;
    }

    // return average voltage
    return (ulSum/NO_OF_ADC_SAMPLES);
}

/********************************************************************************
                   Email - done using SDK 1.2 "email" as an example
********************************************************************************/

//*****************************************************************************
//
// Sets eMail parameters except the message body
//
// return parameter:  0 = success, -1 = failed
//
//*****************************************************************************
int32_t SetEmailParameters(void)
{
    SlNetAppEmailOpt_t eMailServerSetting;
    SlNetAppDestination_t destEmailAdd;
    SlNetAppEmailSubject_t emailSubject;
    SlNetAppSourceEmail_t sourceEmailId;
    SlNetAppSourcePassword_t sourceEmailPwd;

    int32_t lRetVal = -1;

    //Set Destination Email
    memcpy(destEmailAdd.Email,RECIPIENT_EMAIL,strlen(RECIPIENT_EMAIL)+1);
    lRetVal = sl_NetAppEmailSet(SL_NET_APP_EMAIL_ID,NETAPP_DEST_EMAIL, \
                                    strlen(RECIPIENT_EMAIL)+1,
                                    (unsigned char *)&destEmailAdd);
    ASSERT_ON_ERROR(lRetVal);

    //Set Subject Line
    memcpy(emailSubject.Value,EMAIL_SUBJECT,strlen(EMAIL_SUBJECT)+1);
    lRetVal = sl_NetAppEmailSet(SL_NET_APP_EMAIL_ID,NETAPP_SUBJECT, \
                                strlen(EMAIL_SUBJECT)+1,
                                (unsigned char *)&emailSubject);
    ASSERT_ON_ERROR(lRetVal);

    // Set sender email address
    memcpy(sourceEmailId.Username,SENDER_EMAIL_ADD,strlen(SENDER_EMAIL_ADD)+1);
    lRetVal = sl_NetAppEmailSet(SL_NET_APP_EMAIL_ID,NETAPP_SOURCE_EMAIL, \
                                strlen(SENDER_EMAIL_ADD)+1,
                                (unsigned char*)&sourceEmailId);
    ASSERT_ON_ERROR(lRetVal);

    // Set sender email password
    memcpy(sourceEmailPwd.Password,SENDER_EMAIL_PASS,strlen(SENDER_EMAIL_PASS)+1);
    lRetVal = sl_NetAppEmailSet(SL_NET_APP_EMAIL_ID,NETAPP_PASSWORD, \
                                strlen(SENDER_EMAIL_PASS)+1,
                                (unsigned char*)&sourceEmailPwd);
    ASSERT_ON_ERROR(lRetVal);

    // Set Advanced eMAil parameters
    eMailServerSetting.Family = AF_INET;
    eMailServerSetting.Port = GMAIL_HOST_PORT;
    eMailServerSetting.Ip = SL_IPV4_VAL(74,125,129,108);
    eMailServerSetting.SecurityMethod = SL_SO_SEC_METHOD_TLSV1_2;
    eMailServerSetting.SecurityCypher = SL_SEC_MASK_TLS_RSA_WITH_AES_256_CBC_SHA;

    lRetVal = sl_NetAppEmailSet(SL_NET_APP_EMAIL_ID,NETAPP_ADVANCED_OPT, \
                                sizeof(SlNetAppEmailOpt_t), \
                                (unsigned char*)&eMailServerSetting);
    ASSERT_ON_ERROR(lRetVal);


   return SUCCESS;
}

//*****************************************************************************
//
// connects to AP, sends eMail, disconnects from AP
//
// return parameter:  0 = success, -1 = AP connection failed, -2 = email failure
//
//*****************************************************************************
int32_t SendEmail(char * msg)
{
    SlNetAppServerError_t sEmailErrorInfo;
    int32_t lRetCode = SL_EMAIL_ERROR_FAILED;
    int32_t lRetVal = -1;
    signed char g_cConnectStatus;

    //***********************************************************************//
    // Initialize Network Processor
    //
    lRetVal = Network_IF_InitDriver(ROLE_STA);
    if(lRetVal < 0)
    {
        DBG_PRINT("Failed to start SimpleLink Device\n\r");
    }
    //***********************************************************************//
    //Set Default Parameters for Email
    //
    lRetVal = SetEmailParameters();
    if(lRetVal < 0)
    {
        DBG_PRINT("Failed to set default params for Email\r\n");
    }
    //***********************************************************************//
    // Connect to AP
    //
    if(!IS_CONNECTED(Network_IF_CurrentMCUState()))
    {
        lRetVal = Network_IF_ConnectAP(SSID_NAME,SecurityParams);
        if(lRetVal < 0)
        {
           DBG_PRINT("Error: %d Connecting to an AP.\n\r",lRetVal);
           return AP_CONNECT_ERROR;
        }
    }
    //***********************************************************************//
    // Connect to eMail server
    //
    g_cConnectStatus = sl_NetAppEmailConnect();
    // If return -1, throw connect error
    if(g_cConnectStatus == -1)
    {
        DBG_PRINT("\t\t Connection with Email Server Failed, Check Internet Connection and Retry\n\r");
        return EMAIL_SEND_ERROR;
    }
    // If return -2, throw socket option error
    if(g_cConnectStatus == -2)
    {
        DBG_PRINT("\t\t ERROR in socket option\n\r");
        return EMAIL_SEND_ERROR;
    }
    //***********************************************************************//
    // Send eMail
    //
    if(g_cConnectStatus == 0)
    {
        if((lRetCode = sl_NetAppEmailSend(RECIPIENT_EMAIL,EMAIL_SUBJECT,\
                                     msg, \
                                     &sEmailErrorInfo)) == SL_EMAIL_ERROR_NONE)
        {
            DBG_PRINT("\t\t Email Sent!\n\r");
        }
        else
        {
            lRetVal = eMailErrorHadler(lRetCode,(char*)sEmailErrorInfo.Value);
            return EMAIL_SEND_ERROR;
        }
    }
    //***********************************************************************//
    // Stop the NETWORK driver
    //
    Network_IF_DeInitDriver();

    DBG_PRINT("Send Email Task End\n\r");

    return SUCCESS;
}

//*****************************************************************************
//
//    eMailErrorHadler
//
// Performs Error Handling for SMTP State Machine
//
// input parameter:  servermessage is the response buffer from the smtp server
//
// return parameter: 0 on success or -1 on failure
//
//*****************************************************************************
int32_t eMailErrorHadler(int32_t error, char * servermessage)
{
    // Errors are handled via flags set in the smtpStateMachine
    switch(error)
    {
        case SL_EMAIL_ERROR_INIT:
                // Server connection could not be established
                DBG_PRINT((char*)"Server connection error.\r\n");
                break;

        case SL_EMAIL_ERROR_HELO:
                // Server did not accept the HELO command from server
                DBG_PRINT((char*)"Server did not accept HELO:\r\n");
                DBG_PRINT((char*)servermessage);
                break;

        case SL_EMAIL_ERROR_AUTH:
                // Server did not accept authorization credentials
                DBG_PRINT((char*)"Authorization unsuccessful, check username/"
                       "password.\r\n");
                DBG_PRINT((char*)servermessage);
                break;

        case SL_EMAIL_ERROR_FROM:
                // Server did not accept source email.
                DBG_PRINT((char*)"Email of sender not accepted by server.\r\n");
                DBG_PRINT((char*)servermessage);
                break;

        case SL_EMAIL_ERROR_RCPT:
                // Server did not accept destination email
                DBG_PRINT((char*)"Email of recipient not accepted by server.\r\n");
                DBG_PRINT((char*)servermessage);
                break;

        case SL_EMAIL_ERROR_DATA:
                // 'DATA' command to server was unsuccessful
                DBG_PRINT((char*)"smtp 'DATA' command not accepted by server.\r\n");
                DBG_PRINT((char*)servermessage);
                break;

        case SL_EMAIL_ERROR_MESSAGE:
                // Message body could not be sent to server
                DBG_PRINT((char*)"Email Message was not accepted by the server.\r\n");
                DBG_PRINT((char*)servermessage);
                break;

        case SL_EMAIL_ERROR_QUIT:
                // Message could not be finalized
                DBG_PRINT((char*)"Connection could not be properly closed. Message not sent.\r\n");
                DBG_PRINT((char*)servermessage);
                break;

        default:
          break;
    }

    DBG_PRINT("\r\n");

    return SUCCESS;

}

//*****************************************************************************
//
// Put SoC in hibernate mode
//
//
//*****************************************************************************
void Hibernate(void)
{
    DBG_PRINT("\n\n\n\r");
    DBG_PRINT("\t\t ***************  Going HIB  *****************\n\r");
    DBG_PRINT("\n\n\n\r");

    MAP_UtilsDelay(1000*DELAY_1MS); // give it chance to print and show green LED before HIB


    // LED = OFF
    GPIO_IF_LedOff(MCU_ALL_LED_IND);

    // bye-bye
    MAP_PRCMHibernateEnter();
}

//*****************************************************************************
//
// Check return status from send email function
//
//
//*****************************************************************************
void CheckStatus(int32_t status)
{
    if(status == SUCCESS)
    {
        GPIO_IF_LedOn(MCU_GREEN_LED_GPIO); // GREEN led=on
        GPIO_IF_LedOn(MCU_ORANGE_LED_GPIO); // orange led=on
    }
    else if (status == AP_CONNECT_ERROR)
    {
        Flash_LED(MCU_ORANGE_LED_GPIO); // AP error
    }
    else if (status == EMAIL_SEND_ERROR)
    {
        Flash_LED(MCU_GREEN_LED_GPIO); // EMAIL error
    }
}

//*****************************************************************************
//
// Toggles LED passed as argument
//
//
//*****************************************************************************
void Flash_LED(unsigned char LED)
{
    int32_t i;

    for(i=0; i<100; i++)
    {
        GPIO_IF_LedToggle(LED);
        MAP_UtilsDelay(100*DELAY_1MS);
    }
    GPIO_IF_LedOff(LED);
}
//*****************************************************************************
//
// End !!!
//
//*****************************************************************************
