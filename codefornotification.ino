// This #include statement was automatically added by the Particle IDE.
#include "S3B.h"

// This #include statement was automatically added by the Particle IDE.
#include <NCD2Relay.h>

#include "softap_http.h"

//Set Photon antenna connection to external antenna.  Not absolutely required in most instances.


//Do not attempt to connect to WiFi AP or Particle cloud on boot.
// SYSTEM_MODE(MANUAL);

//Global variables for flasher timer.  Default will be 30 seconds or 30,000 milliseconds.
unsigned long tDuration = 30000;
unsigned long tActivated;

//Object reference to relay board.
NCD2Relay relay;

//Local functions.
void command(int id);
void tConfig(int len);

//Variables used for reading on board inputs.
bool tripped[6];

int debugTrips[6];

int minTrips = 5;

int nDevices;

byte deviceAddresses[12][8];

S3B wireless;

void parseReceivedData();
byte receiveBuffer[256];
unsigned long tOut = 3000;

int moduleRSSI = 0;

//Soft AP variables.
struct Page
{
	const char* url;
	const char* mime_type;
	const char* data;
};

const char index_html[] = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><title>Flasher Duration</title><script type='text/javascript' src='scripts/values.js'></script></head><body><form><label>Please Enter Flasher Duration(Minimum 10, Maximum 120)</label><input type='numeric' name='duration' id='duration' value='' /><input type='submit' value='Submit' /></form><script type='text/javascript'>(function () {'use strict';document.getElementByID('duration').value=duration;})();</script></body></html>";

const char group_members_html[] = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><title>Devices</title></head><body><form><label>Remote Node Addresses</label><br><input type='numeric' name='node1' id='node1' value='' /><br><input type='numeric' name='node2' id='node2' value='' /><br><input type='numeric' name='node3' id='node3' value='' /><br><input type='numeric' name='node4' id='node4' value='' /><br><input type='numeric' name='node5' id='node5' value='' /><br><input type='numeric' name='node6' id='node6' value='' /><br><input type='numeric' name='node7' id='node7' value='' /><br><input type='numeric' name='node8' id='node8' value='' /><br><input type='numeric' name='node9' id='node9' value='' /><br><input type='numeric' name='node10' id='node10' value='' /><br><input type='numeric' name='node11' id='node11' value='' /><br><input type='numeric' name='node12' id='node12' value='' /><br><input type='submit' value='Submit' /></form></body><script type='text/javascript'>(function () {'use strict';document.getElementByID('node1').value=node1;document.getElementByID('node2').value=node2;document.getElementByID('node3').value=node3;document.getElementByID('node4').value=node4;document.getElementByID('node5').value=node5;document.getElementByID('node6').value=node6;document.getElementByID('node7').value=node7;document.getElementByID('node8').value=node8;document.getElementByID('node9').value=node9;document.getElementByID('node10').value=node10;document.getElementByID('node11').value=node11;document.getElementByID('node12').value=node12;})();</script></html>";

Page myPages[] = {
		{ "/index", "text/html", index_html},
		{"/devices", "text/html", group_members_html},
		{ nullptr }
};
void myPage(const char* url, ResponseCallback* cb, void* cbArg, Reader* body, Writer* result, void* reserved);
STARTUP(softap_set_application_page_handler(myPage, nullptr));

void setup() {
    //Open Serial1 port for interfacing through S3B module.
    Serial1.begin(115200);
    //Initialize relay controller with status of on board address Jumpers
    relay.setAddress(0,0,0);
    //Get stored timer duration from memory
    int storedTDuration;
    EEPROM.get(0, storedTDuration);
    if(storedTDuration > 0 && storedTDuration < 121){
        tDuration = storedTDuration*1000;
    }
    loadDevicesFromMemory();
    
    if(Particle.connected()){
        Particle.variable("RSSI", moduleRSSI);
    }
    
}

void loop() {
    //Check for command received through S3B
    if(Serial1.available() > 0){
        parseReceivedData();
        moduleRSSI = wireless.getRSSI();
    }
    
    //read on boad inputs and react to them closing.
    int status = relay.readAllInputs();
	int a = 0;
	for(int i = 1; i < 33; i*=2){
		if(status & i){
			debugTrips[a]++;
			if(debugTrips[a] >= minTrips){
				if(!tripped[a]){
					tripped[a] = true;
					int inputNumber = a+1;
					switch(inputNumber){
					    case 1 : 
					    //Input 1 tripped so send transmission to other units and execute command locally
                        command(1, true);
                        break;
                        case 2 :
                        //Input 2 tripped so turn off relay, then go to soft AP mode for timer configuration.
			if(!status & 1){
                             command(0, true);
			}
                        break;
                        case 3 :
                        //Input 3 tripped so turn off relay, then go into safe mode.
                        
                       
                        break;
					}
				}
			}
		}else{
			debugTrips[a] = 0;
			if(tripped[a]){
				tripped[a] = false;
				int inputNumber = a+1;
			}
		}
		a++;
	}
    
    //Check relay Timer
    if(millis() > tDuration+tActivated && tActivated != 0){
        
        tActivated = 0;
    }
}

//Command handler
void command(int id, bool transmit){
    if(id == 1){
        //Start relay timer to flash light
        relay.turnOnRelay(1);
      
    }
    if(id == 0){
relay.turnOffRelay(1);
}
    //Send command to remote units.
if(transmit){
        Serial.printf("nDevices = %i\n", nDevices);
        for(int i = 0; i < nDevices; i++){
          byte commandData[1] ={id};
         if(!wireless.transmit(deviceAddresses[i], commandData, 1)){
                //Handle failed transmission
                if(Particle.connected()){
                    char buffer[25];
                    hexString(deviceAddresses[i], 8, buffer);
                    String info = String(buffer);
                    Particle.publish("TxFail", info);
                    Serial.printf("Transmission failed to: %s\n",buffer);
                }
                // Serial.println("Transmission failed");
            }
    }
    }
}

//Timer config
void tConfig(int len){
    //set timer global variable(multiply by 1000 since we use millis
    tDuration = len*1000;
    //Store timer variable to memory
    EEPROM.put(0, len);
}

//Soft AP web interface.
void myPage(const char* url, ResponseCallback* cb, void* cbArg, Reader* body, Writer* result, void* reserved)
{
	Serial.printlnf("handling page %s", url);
	String u = String(url);
	
	int8_t idx = 0;
	for (;;idx++) {
		Page& p = myPages[idx];
		if (!p.url) {
			idx = -1;
			break;
		}
		else if (strcmp(url, p.url)==0) {
			break;
		}
	}
	Serial.printf("Page Index: %i \n",idx);
	if(idx==-1){
		if(strstr(url,"values.js")){
			Serial.println("values.js started");
            int EEPROM_duration;
            
            EEPROM.get(0, EEPROM_duration);
			//Stored Duration
			String duration = "duration=\"";
			duration.concat(EEPROM_duration);
			duration.concat("\";");

			cb(cbArg, 0, 200, "application/javascript", nullptr);
			result->write(duration);
			Serial.println("devices.js finished");
		}
		if(strstr(url,"index")){
			//This runs on submit
			if(strstr(url,"?")){
				Serial.println("index? started");
				String parsing = String(url);
				
				int start = parsing.indexOf("duration=");
				parsing = parsing.substring(start+9);
				
				int duration = parsing.toInt();
				if(duration > 9 && duration < 121){
				    Serial.printf("Timer duration: %i\n", duration);
				    //Send command to remote units.
                    for(int i = 0; i < nDevices; i++){
                        byte commandData[1] = {(byte)duration};
                        if(!wireless.transmit(deviceAddresses[i], commandData, 1)){
                            //Handle failed transmission
				char dID[25];
                            hexString(deviceAddresses[i], 8, dID);
                            Serial.printf("Transmission failed to device: %s\n", dID);
                        }else{
                            //Transmission Succsess.
                        }
                    }
				    EEPROM.put(0, duration);
				    delay(1000);
				}
					
					
				System.reset();
					
				Serial.println("sending redirect");
				Header h("Location: /complete\r\n");
				cb(cbArg, 0, 302, "text/plain", &h);
				return;
			}
				else{
					Serial.println("oops, something went wrong on index");
					Serial.println("404 error");
					cb(cbArg, 0, 404, nullptr, nullptr);
				}
		}
		if(strstr(url, "devices")){
		    Serial.println("Devices page loaded");
		    if(strstr(url, "?")){
		        int eepromReg = 9;
		        
		        Serial.println("Devices page Submit.");
		        Serial.println(url);
		        String parsing = String(url);
		        
		        int numberOfDevices = 0;
		        int start;
		        int stop;
		        for(int i = 0; i < 12; i++){
		            char nodeID[7];
		            sprintf(nodeID, "node%i=", i+1);
		            start = (parsing.indexOf(nodeID));
		            if(i > 8){
		                start = start+7;
		            }else{
		                start = start+6;
		            }
		            String nodeValue;
		            if(i < 11){
		                stop = parsing.indexOf("&", start);
		                nodeValue = parsing.substring(start, stop);
		            }else{
		                nodeValue = parsing.substring(start);
		            }
		            if(nodeValue.startsWith("0013A200")){
		                numberOfDevices++;
		                byte addressBytes[8];
		                hexStringToHex(nodeValue, addressBytes);
		                for(int j = 0; j < 8; j++){
		                    EEPROM.write(eepromReg+j, addressBytes[j]);
		                }
		                eepromReg+=8;
		            }
		        }
		        Serial.printf("Storing %i devices\n", numberOfDevices);
		        for(int i = 0; i < numberOfDevices; i++){
		            Serial.printf("Device %i: ", i+1);
		            for(int j = 0; j < 8; j++){
		                Serial.printf("%02X:", EEPROM.read(9+(i*8)+j));
		            }
		            Serial.println();
		        }
		        EEPROM.put(5, numberOfDevices);
		        
		        delay(1000);
		        System.reset();
		        
		    }
		}
	}else{
		Serial.println("Normal page request");
		cb(cbArg, 0, 200, myPages[idx].mime_type, nullptr);
		result->write(myPages[idx].data);
		Serial.println("result written");
	}
	return;
}

byte b[4];
//char msg[] = "13ABF2C1";
char msg[] = "13abf2c1";

void hexStringToHex(String s, byte* buffer) {
    char stringChar[s.length()+1];
    s.toCharArray(stringChar, s.length()+1);
    int incB = 0;
    for(int i = 0; i < s.length()+1; i+=2){
        Serial.printf("evaluation Chars(int): %i, %i\n", stringChar[i], stringChar[i+1]);
        Serial.printf("evaluation Chars(char): %c, %c\n", stringChar[i], stringChar[i+1]);
        byte a;
        byte b;
        if(stringChar[i] > 64 && stringChar[i] < 71){
            a = stringChar[i]-55;
        }
        if(stringChar[i] >96 && stringChar[i] < 103){
            a = stringChar[i]-87;
        }
        if(stringChar[i] > 47 && stringChar[i] < 58){
            a = stringChar[i]-48;
        }
        if(stringChar[i+1] > 64 && stringChar[i+1] < 71){
            b = stringChar[i+1]-55;
        }
        if(stringChar[i+1] >96 && stringChar[i+1] < 103){
            b = stringChar[i+1]-87;
        }
        if(stringChar[i+1] > 47 && stringChar[i+1] < 58){
            b = stringChar[i+1]-48;
        }
        buffer[incB] = (a*16)+b;

        incB++;
    }
}

void hexString(byte* data,  size_t len, char buffer[]){
	sprintf(buffer, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

void loadDevicesFromMemory(){
    int numberOfDevices;
    EEPROM.get(5, numberOfDevices);
    nDevices = numberOfDevices;
    for(int i = 0; i < numberOfDevices; i++){
        Serial.printf("Stored Device %i: ", i+1);
        for(int j = 0; j < 8; j++){
            deviceAddresses[i][j] = EEPROM.read((8*i)+j+9);
            Serial.printf("%02X:", deviceAddresses[i][j]);
        }
        Serial.println();
    }
    
}

void parseReceivedData(){
    // Serial.println("Got a command");
    byte startDelimiter = Serial1.read();
    if(startDelimiter == 0x7E){
        unsigned long startTime = millis();
        while(Serial1.available() < 2 && millis() <= tOut+startTime);
        if(Serial1.available() < 2){
            Serial.println("Timeout");
            return;
        }
        byte lenMSB = Serial1.read();
        byte lenLSB = Serial1.read();
        int newDataLength = (lenMSB*256)+lenLSB;
        
        int count = 0;
        while(count != newDataLength+1 && millis() <= tOut+startTime){
            if(Serial1.available() != 0){
                receiveBuffer[count] = Serial1.read();
                count++;
            }
        }
        if(count < newDataLength+1){
            Serial.println("Timeout2");
            Serial.printf("Received Bytes: %i, expected %i \n", count, newDataLength+1);
            return;
        }
        // Serial.printf("Received %i bytes \n", count);
        //We have all our data.
        byte newData[newDataLength+4];
        newData[0] = startDelimiter;
        newData[1] = lenMSB;
        newData[2] = lenLSB;
        for(int i = 3; i < newDataLength+4; i++){
            newData[i] = receiveBuffer[i-3];
        }
        // Serial.print("Received: ");
        // for(int i = 0; i < sizeof(newData); i++){
        //     Serial.printf("%x, ", newData[i]);
        // }
        // Serial.println();
        //validate data
        if(!wireless.validateReceivedData(newData, newDataLength+4)){
            Serial.println("Invalid Data");
            return;
        }
        //get length of new data
        int receivedDataLength = wireless.getReceiveDataLength(newData);
        char receivedData[receivedDataLength];
        int validDataCount = wireless.parseReceive(newData, receivedData, newDataLength+4);
        if(validDataCount == receivedDataLength){

        }
        if(receivedData[0] == 1 || receivedData[0] == 0){
            command(receivedData[0], false);
        }else{
            if(receivedData[0] > 10 && receivedData[0] < 120){
                Serial.printf("storing new timer duration of %i seconds\n", receivedData[0]);
                tConfig(receivedData[0]);
            }
        }
        
        
    }else{
        Serial.printf("First byte not valid, it was: 0x%x \n", startDelimiter);
    }
}
