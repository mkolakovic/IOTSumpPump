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
#ifndef __CONFIG_H
#define __CONFIG_H

/********************************************************************************
    AP connection
********************************************************************************/
#define MY_SSID_NAME            "net"                 // AP SSID
#define MY_SECURITY_TYPE        SL_SEC_TYPE_WPA_WPA2   // Security type (OPEN or WEP or WPA)
#define SECURITY_KEY            "net pass"          // secured AP PAssword



//*******************************************************************************
//   eMail
//*******************************************************************************
#define GMAIL_HOST_PORT         465
#define EMAIL_SUBJECT           "Urgent !!!"

#define SENDER_EMAIL_ADD        "your_gmail@gmail.com" //Set Sender/Source Email Address
#define SENDER_EMAIL_PASS       "your_gmail_pass"                //Set Sender/Source Email Password
#define RECIPIENT_EMAIL         "your_phone#@vtext.com"    //Set Destination Email Address



#endif
